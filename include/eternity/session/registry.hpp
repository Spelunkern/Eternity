#pragma once
#include "eternity/net/login_crypto.hpp"
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

namespace eternity::session {
struct LoginSession {
    std::uint32_t user_id;
    std::uint64_t session_id;
    std::uint8_t permission;
    std::string username;
    std::array<std::uint8_t, 16> token;
    std::array<std::uint8_t, 32> game_key;
};

// CUserMgr::AddUser (ps_session.exe 00409700) replaces an existing account session.
// The caller disconnects the returned owner after releasing the registry lock.
class Registry {
public:
    std::optional<LoginSession> add(LoginSession session);
    std::optional<LoginSession> find(std::uint32_t user_id) const;
    bool remove(std::uint32_t user_id, std::uint64_t session_id);
    std::size_t size() const;
private:
    mutable std::mutex mutex_;
    std::unordered_map<std::uint32_t, LoginSession> sessions_;
};

std::array<std::uint8_t, 32> game_key(const net::SessionSecrets& secrets);
}
