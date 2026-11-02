#pragma once
#include "eternity/net/login_crypto.hpp"
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <functional>
#include <vector>

namespace eternity::session {
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
};

// CUserMgr::AddUser (ps_session.exe 00409700) replaces an existing account session.
// The caller disconnects the returned owner after releasing the registry lock.
class Registry {
public:
    std::optional<LoginSession> add(LoginSession session);
    std::optional<LoginSession> find(std::uint32_t user_id) const;
    bool remove(std::uint32_t user_id, std::uint64_t session_id);
    bool release_login(std::uint32_t user_id,std::uint64_t session_id);
    bool select(std::uint32_t user_id,std::uint64_t session_id,std::uint8_t realm,std::uint64_t now_ms);
    std::optional<LoginSession> claim(std::uint32_t user_id,const std::array<std::uint8_t,16>& token,
        std::uint8_t realm,std::uint64_t connection,std::uint64_t now_ms,std::function<void()> disconnect = {});
    bool release_world(std::uint32_t user_id,std::uint64_t session_id,std::uint64_t connection);
    std::vector<LoginSession> expire(std::uint64_t now_ms);
    std::size_t size() const;
private:
    mutable std::mutex mutex_;
    std::unordered_map<std::uint32_t, LoginSession> sessions_;
};

std::array<std::uint8_t, 32> game_key(const net::SessionSecrets& secrets);
}
