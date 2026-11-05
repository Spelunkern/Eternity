#pragma once
#include "eternity/net/world_crypto.hpp"
#include "eternity/session/registry.hpp"
#include <optional>

namespace eternity::net {
struct WorldEvent {
    std::vector<Packet> replies;
    std::optional<Packet> application;
    // Raw, decrypted A302; the retained connection owner must invoke its real NProtect backend.
    std::optional<Packet> nprotect;
    bool connected = false;
    bool close = false;
    std::optional<std::uint32_t> close_reason;
};

// Socket-independent game connection state. All calls belong to one socket executor.
class WorldProtocol {
public:
    enum class State { connecting, character_selection, world, closed };
    WorldProtocol(std::shared_ptr<session::Registry> registry, std::uint8_t realm,
        std::uint64_t connection, std::shared_ptr<const WorldXor> world_xor,
        std::function<void()> disconnect = {});
    ~WorldProtocol();
    WorldProtocol(const WorldProtocol&) = delete;
    WorldProtocol& operator=(const WorldProtocol&) = delete;
    // retained_world_live is the optional external live-index result; true
    // denies a pre-authenticated duplicate even after login-session replacement.
    WorldEvent receive(Packet payload, std::uint64_t now_ms,bool retained_world_live=false);
    Packet outgoing(Packet payload);
    // Original timeout check with the zero baseline observed for fresh sockets.
    bool reply_timed_out(std::uint64_t now_ms) const noexcept;
    // True at admission location 3 and retained through location 4; false at selection.
    // Projection of the admission owner, independent of the outgoing cipher.
    // True forwards all payloads; the application must apply the real location gate.
    void set_admission_dispatch(bool enabled);
    void enter_world();
    void enter_character_selection();
    void close() noexcept;
    State state() const noexcept { return state_; }
    const std::optional<session::LoginSession>& owner() const noexcept { return owner_; }
private:
    std::shared_ptr<session::Registry> registry_;
    std::uint8_t realm_;
    std::uint64_t connection_;
    std::shared_ptr<const WorldXor> world_xor_;
    std::function<void()> disconnect_;
    std::optional<session::LoginSession> owner_;
    std::unique_ptr<WorldCipher> cipher_;
    State state_ = State::connecting;
    bool admission_dispatch_ = false;
    unsigned int undefined_packets_ = 0;
    std::uint32_t last_reply_tick_ = 0;
};
}
