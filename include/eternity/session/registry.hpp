#pragma once
#include "eternity/net/login_crypto.hpp"
#include <cstdint>
#include <mutex>
#include <memory>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <functional>
#include <vector>

namespace eternity::session {
class WorldReservation {
public:
    std::uint32_t account() const noexcept { return account_; }
    std::uint8_t realm() const noexcept { return realm_; }
    std::uint64_t session_id() const noexcept { return session_id_; }
    std::uint64_t connection() const noexcept { return connection_; }
    WorldReservation(const WorldReservation&)=delete;
    WorldReservation& operator=(const WorldReservation&)=delete;
private:
    friend class Registry;
    WorldReservation(std::uint32_t account,std::uint8_t realm,std::uint64_t session_id,std::uint64_t connection)
        :account_(account),realm_(realm),session_id_(session_id),connection_(connection) {}
    std::uint32_t account_;
    std::uint8_t realm_;
    std::uint64_t session_id_;
    std::uint64_t connection_;
};

// A single retained-world owner holds this distinct capability. The issuing
// registry keeps it alive if the owner registry is abandoned unexpectedly.
class WorldAdoption {
public:
    const std::shared_ptr<const WorldReservation>& reservation() const noexcept {return reservation_;}
    WorldAdoption(const WorldAdoption&)=delete;
    WorldAdoption& operator=(const WorldAdoption&)=delete;
private:
    friend class Registry;
    explicit WorldAdoption(std::shared_ptr<const WorldReservation> reservation):reservation_(std::move(reservation)) {}
    std::shared_ptr<const WorldReservation> reservation_;
};

struct LoginSession {
    std::uint32_t user_id;
    std::uint64_t session_id;
    std::uint8_t permission;
    std::string username;
    std::array<std::uint8_t, 16> token;
    std::array<std::uint8_t, 32> game_key;
    enum class Location { login, handoff, world };
    Location location = Location::login;
    std::uint8_t realm = 0xff;
    std::uint64_t handoff_deadline = 0;
    std::uint64_t world_connection = 0;
    std::function<void()> disconnect = {};
    // Present only on a successful world claim. Retain through all owner/DB drain.
    std::shared_ptr<const WorldReservation> world_reservation;
};

// Issued under the registry lock while transferring a disconnect request. The
// registry retains it until identity-checked release after actual recovery.
// Metadata identifies the previous owner without exposing tokens or keys.
class LoginDisconnectFence {
public:
    std::uint32_t account() const noexcept {return account_;}
    std::uint64_t session_id() const noexcept {return session_id_;}
    LoginSession::Location location() const noexcept {return location_;}
    std::uint8_t realm() const noexcept {return realm_;}
    std::uint64_t world_connection() const noexcept {return world_connection_;}
    enum class Request {issued,already_issued,in_flight,retired};
    // Calls the exact transferred callback outside the registry lock. Only one
    // call runs at once; an exception leaves it retryable. Already-issued is
    // returned only after a real earlier callback returned successfully.
    Request request_disconnect() const;
    LoginDisconnectFence(const LoginDisconnectFence&)=delete;
    LoginDisconnectFence& operator=(const LoginDisconnectFence&)=delete;
private:
    friend class Registry;
    explicit LoginDisconnectFence(const LoginSession& owner)
        :account_(owner.user_id),session_id_(owner.session_id),location_(owner.location),realm_(owner.realm),world_connection_(owner.world_connection),disconnect_(owner.disconnect) {}
    std::uint32_t account_;
    std::uint64_t session_id_;
    LoginSession::Location location_;
    std::uint8_t realm_;
    std::uint64_t world_connection_;
    std::function<void()> disconnect_;
    enum class State {ready,in_flight,issued,retired};
    mutable std::mutex request_mutex_;
    mutable State request_state_=State::ready;
    bool retire() const;
};

// CUserMgr::AddUser (ps_session.exe 00409700) replaces an existing account session.
// The caller disconnects the returned owner after releasing the registry lock.
class Registry {
public:
    std::optional<LoginSession> add(LoginSession session);
    struct LoginAddResult {
        bool blocked=false;
        std::optional<LoginSession> previous;
        std::shared_ptr<const LoginDisconnectFence> disconnect_fence;
    };
    // For login owners issuing the returned previous owner's real disconnect.
    // Retain the fence until that request returns or actual recovery completes.
    LoginAddResult add_with_disconnect_fence(LoginSession session);
    // Release only after the real disconnect request returns or actual recovery.
    bool release_login_disconnect_fence(const std::shared_ptr<const LoginDisconnectFence>& fence);
    // Recovery inspection; a snapshot is not evidence that cleanup occurred.
    std::vector<std::shared_ptr<const LoginDisconnectFence>> login_disconnect_fences() const;
    std::optional<LoginSession> find(std::uint32_t user_id) const;
    bool remove(std::uint32_t user_id, std::uint64_t session_id);
    bool release_login(std::uint32_t user_id,std::uint64_t session_id);
    // Selection and world claims reject while any prior disconnect request for
    // this account remains fenced in this registry.
    bool select(std::uint32_t user_id,std::uint64_t session_id,std::uint8_t realm,std::uint64_t now_ms);
    std::optional<LoginSession> claim(std::uint32_t user_id,const std::array<std::uint8_t,16>& token,
        std::uint8_t realm,std::uint64_t connection,std::uint64_t now_ms,std::function<void()> disconnect = {});
    struct ClaimResult { std::optional<LoginSession> session; bool duplicate = false; };
    ClaimResult claim_world(std::uint32_t user_id,const std::array<std::uint8_t,16>& token,
        std::uint8_t realm,std::uint64_t connection,std::uint64_t now_ms,std::function<void()> disconnect = {});
    bool world_reserved(std::uint32_t user_id,std::uint8_t realm) const;
    // True only for the live token object issued by this registry.
    bool owns_world_reservation(const std::shared_ptr<const WorldReservation>& reservation) const;
    // Each token can be adopted once. Release only after actual owner
    // cleanup and all associated DB/reply work have drained.
    std::shared_ptr<const WorldAdoption> adopt_world(const std::shared_ptr<const WorldReservation>& reservation);
    bool release_world_adoption(const std::shared_ptr<const WorldAdoption>& adoption);
    // For inspection/recovery of abandoned owners; snapshots retain the token.
    std::vector<std::shared_ptr<const WorldAdoption>> world_adoptions() const;
    // A failed factory or close leaves cleanup uncertain. Retain the exact
    // claim until the coordinator has actually completed recovery and drain.
    bool retain_failed_world(const std::shared_ptr<const WorldReservation>& reservation);
    std::vector<std::shared_ptr<const WorldReservation>> failed_world_reservations() const;
    // The caller must have completed real owner cleanup and all DB/reply drain.
    // Existing shared copies continue to fence the account after this call.
    bool release_failed_world(const std::shared_ptr<const WorldReservation>& reservation);
    bool release_world(std::uint32_t user_id,std::uint64_t session_id,std::uint64_t connection);
    std::vector<LoginSession> expire(std::uint64_t now_ms);
    struct ExpiredHandoff {
        LoginSession owner;
        std::shared_ptr<const LoginDisconnectFence> disconnect_fence;
    };
    // Atomically remove expired handoffs and fence each returned disconnect.
    // Accounts already fenced by another request remain for a later sweep.
    std::vector<ExpiredHandoff> expire_with_disconnect_fences(std::uint64_t now_ms);
    std::size_t size() const;
private:
    mutable std::mutex mutex_;
    std::unordered_map<std::uint32_t, LoginSession> sessions_;
    std::map<std::uint32_t,std::shared_ptr<const LoginDisconnectFence>> login_disconnect_fences_;
    struct WorldReservationEntry {
        std::weak_ptr<const WorldReservation> active;
        std::shared_ptr<const WorldReservation> failed;
        std::shared_ptr<const WorldAdoption> adoption;
        bool adoption_spent=false;
    };
    std::map<std::pair<std::uint8_t,std::uint32_t>,WorldReservationEntry> world_reservations_;
};

std::array<std::uint8_t, 32> game_key(const net::SessionSecrets& secrets);
}
