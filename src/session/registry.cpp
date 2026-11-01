#include "eternity/session/registry.hpp"
#include <algorithm>
#include <openssl/evp.h>

namespace eternity::session {
std::optional<LoginSession> Registry::add(LoginSession session) {
    if (session.user_id == 0 || session.session_id == 0)
        throw std::invalid_argument("A session needs nonzero account and session identifiers");
    std::lock_guard lock(mutex_);
    const auto current = sessions_.find(session.user_id);
    std::optional<LoginSession> previous;
    if (current != sessions_.end()) previous = current->second;
    sessions_.insert_or_assign(session.user_id, std::move(session));
    return previous;
}

std::optional<LoginSession> Registry::find(std::uint32_t user_id) const {
    std::lock_guard lock(mutex_);
    const auto found = sessions_.find(user_id);
    return found == sessions_.end() ? std::nullopt : std::optional(found->second);
}

bool Registry::remove(std::uint32_t user_id, std::uint64_t session_id) {
    std::lock_guard lock(mutex_);
    const auto found = sessions_.find(user_id);
    if (found == sessions_.end() || found->second.session_id != session_id) return false;
    sessions_.erase(found);
    return true;
}

std::size_t Registry::size() const {
    std::lock_guard lock(mutex_);
    return sessions_.size();
}

std::array<std::uint8_t, 32> game_key(const net::SessionSecrets& secrets) {
    // CUserCrypto::GetKeyGen, ps_login.exe 00404d50: key || SHA256(IV)[0..15].
    std::array<std::uint8_t, 32> result{};
    std::array<std::uint8_t, 32> digest{};
    unsigned int length = 0;
    if (EVP_Digest(secrets.iv.data(), secrets.iv.size(), digest.data(), &length, EVP_sha256(), nullptr) != 1 || length != 32)
        throw std::runtime_error("Generate world session key failed");
    std::copy(secrets.key.begin(), secrets.key.end(), result.begin());
    std::copy_n(digest.begin(), 16, result.begin() + 16);
    return result;
}
}
