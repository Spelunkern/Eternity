#include "eternity/session/registry.hpp"
#include <algorithm>
#include <openssl/evp.h>
#include <openssl/crypto.h>
#include <limits>

namespace eternity::session {
std::optional<LoginSession> Registry::add(LoginSession session) {
    if (session.user_id == 0 || session.session_id == 0)
        throw std::invalid_argument("A session needs nonzero account and session identifiers");
    session.location=LoginSession::Location::login;
    session.realm=0xff;
    session.handoff_deadline=0;
    session.world_connection=0;
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

bool Registry::release_login(std::uint32_t user_id,std::uint64_t session_id) {
    std::lock_guard lock(mutex_);
    const auto found=sessions_.find(user_id);
    if(found==sessions_.end() || found->second.session_id!=session_id ||
       found->second.location!=LoginSession::Location::login) return false;
    sessions_.erase(found);return true;
}

bool Registry::select(std::uint32_t user_id,std::uint64_t session_id,std::uint8_t realm,std::uint64_t now_ms) {
    if(realm>=254 || now_ms>std::numeric_limits<std::uint64_t>::max()-30000) return false;
    std::lock_guard lock(mutex_);
    const auto found=sessions_.find(user_id);
    if(found==sessions_.end() || found->second.session_id!=session_id ||
       found->second.location!=LoginSession::Location::login) return false;
    auto& session=found->second;
    session.location=LoginSession::Location::handoff;
    session.realm=realm;
    session.handoff_deadline=now_ms+30000;
    return true;
}

std::optional<LoginSession> Registry::claim(std::uint32_t user_id,const std::array<std::uint8_t,16>& token,
    std::uint8_t realm,std::uint64_t connection,std::uint64_t now_ms,std::function<void()> disconnect) {
    if(connection==0) return std::nullopt;
    std::lock_guard lock(mutex_);
    const auto found=sessions_.find(user_id);
    if(found==sessions_.end()) return std::nullopt;
    auto& session=found->second;
    if(session.location!=LoginSession::Location::handoff || session.realm!=realm ||
       session.handoff_deadline<=now_ms || CRYPTO_memcmp(token.data(),session.token.data(),16)!=0) return std::nullopt;
    session.location=LoginSession::Location::world;
    session.world_connection=connection;
    session.handoff_deadline=0;
    session.disconnect=std::move(disconnect);
    return session;
}

bool Registry::release_world(std::uint32_t user_id,std::uint64_t session_id,std::uint64_t connection) {
    std::lock_guard lock(mutex_);
    const auto found=sessions_.find(user_id);
    if(found==sessions_.end() || found->second.session_id!=session_id ||
       found->second.location!=LoginSession::Location::world || found->second.world_connection!=connection) return false;
    sessions_.erase(found);return true;
}

std::vector<LoginSession> Registry::expire(std::uint64_t now_ms) {
    std::lock_guard lock(mutex_);
    std::vector<LoginSession> expired;
    for(auto iterator=sessions_.begin();iterator!=sessions_.end();) {
        if(iterator->second.location==LoginSession::Location::handoff && iterator->second.handoff_deadline<=now_ms) {
            expired.push_back(iterator->second);
            iterator=sessions_.erase(iterator);
        } else ++iterator;
    }
    return expired;
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
