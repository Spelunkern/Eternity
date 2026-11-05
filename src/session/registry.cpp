#include "eternity/session/registry.hpp"
#include <algorithm>
#include <openssl/evp.h>
#include <openssl/crypto.h>
#include <limits>

namespace eternity::session {
LoginDisconnectFence::Request LoginDisconnectFence::request_disconnect() const {
    {
        std::lock_guard lock(request_mutex_);
        if(request_state_==State::in_flight)return Request::in_flight;
        if(request_state_==State::issued)return Request::already_issued;
        if(request_state_==State::retired)return Request::retired;
        request_state_=State::in_flight;
    }
    try {disconnect_();}
    catch(...) {std::lock_guard lock(request_mutex_);request_state_=State::ready;throw;}
    std::lock_guard lock(request_mutex_);request_state_=State::issued;return Request::issued;
}
bool LoginDisconnectFence::retire() const {
    std::lock_guard lock(request_mutex_);
    if(request_state_==State::in_flight || request_state_==State::retired)return false;
    request_state_=State::retired;return true;
}

std::optional<LoginSession> Registry::add(LoginSession session) {
    if (session.user_id == 0 || session.session_id == 0)
        throw std::invalid_argument("A session needs nonzero account and session identifiers");
    session.location=LoginSession::Location::login;
    session.realm=0xff;
    session.handoff_deadline=0;
    session.world_connection=0;
    session.world_reservation.reset();
    std::lock_guard lock(mutex_);
    const auto fence=login_disconnect_fences_.find(session.user_id);
    if(fence!=login_disconnect_fences_.end())
        throw std::logic_error("Login account disconnect request is unresolved");
    const auto current = sessions_.find(session.user_id);
    std::optional<LoginSession> previous;
    if (current != sessions_.end()) previous = current->second;
    sessions_.insert_or_assign(session.user_id, std::move(session));
    return previous;
}

Registry::LoginAddResult Registry::add_with_disconnect_fence(LoginSession session) {
    if(!session.user_id || !session.session_id)throw std::invalid_argument("A session needs nonzero account and session identifiers");
    session.location=LoginSession::Location::login;session.realm=0xff;session.handoff_deadline=0;
    session.world_connection=0;session.world_reservation.reset();
    std::lock_guard lock(mutex_);
    auto existing_fence=login_disconnect_fences_.find(session.user_id);
    if(existing_fence!=login_disconnect_fences_.end())return {true,{},{}};
    const auto current=sessions_.find(session.user_id);LoginAddResult result;
    if(current!=sessions_.end()) {
        result.previous=current->second;
        if(current->second.session_id!=session.session_id && current->second.disconnect) {
            result.disconnect_fence=std::shared_ptr<const LoginDisconnectFence>(new LoginDisconnectFence(current->second));
            login_disconnect_fences_.insert_or_assign(session.user_id,result.disconnect_fence);
        }
    }
    try {sessions_.insert_or_assign(session.user_id,std::move(session));}
    catch(...) {
        if(result.disconnect_fence)login_disconnect_fences_.erase(result.disconnect_fence->account());
        throw;
    }
    return result;
}
bool Registry::release_login_disconnect_fence(const std::shared_ptr<const LoginDisconnectFence>& fence) {
    if(!fence)return false;
    std::lock_guard lock(mutex_);const auto found=login_disconnect_fences_.find(fence->account());
    if(found==login_disconnect_fences_.end() || found->second!=fence || !fence->retire())return false;
    login_disconnect_fences_.erase(found);return true;
}
std::vector<std::shared_ptr<const LoginDisconnectFence>> Registry::login_disconnect_fences() const {
    std::lock_guard lock(mutex_);std::vector<std::shared_ptr<const LoginDisconnectFence>> result;
    result.reserve(login_disconnect_fences_.size());
    for(const auto& [id,fence]:login_disconnect_fences_){(void)id;result.push_back(fence);}
    return result;
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
    if(login_disconnect_fences_.contains(user_id))return false;
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
    return claim_world(user_id,token,realm,connection,now_ms,std::move(disconnect)).session;
}

Registry::ClaimResult Registry::claim_world(std::uint32_t user_id,const std::array<std::uint8_t,16>& token,
    std::uint8_t realm,std::uint64_t connection,std::uint64_t now_ms,std::function<void()> disconnect) {
    if(connection==0) return {};
    std::lock_guard lock(mutex_);
    // Weak entries do not own sessions; prune them when admitting another claim.
    for(auto it=world_reservations_.begin();it!=world_reservations_.end();) {
        if(it->second.active.expired()) it=world_reservations_.erase(it); else ++it;
    }
    const auto key=std::pair{realm,user_id};
    if(world_reservations_.contains(key)) return {{},true};
    const auto found=sessions_.find(user_id);
    if(found==sessions_.end()) return {};
    auto& session=found->second;
    if(session.location==LoginSession::Location::world && session.realm==realm) return {{},true};
    // A retained world owner takes precedence over a concurrent login fence,
    // matching CWorld::FindUserBill before the manager admission decision.
    if(login_disconnect_fences_.contains(user_id))return {};
    if(session.location!=LoginSession::Location::handoff || session.realm!=realm ||
       session.handoff_deadline<=now_ms || CRYPTO_memcmp(token.data(),session.token.data(),16)!=0) return {};
    auto reservation=std::shared_ptr<const WorldReservation>(new WorldReservation(user_id,realm,session.session_id,connection));
    auto result=session; // Copy before changing the registry, including any throwing callback copy.
    result.location=LoginSession::Location::world;result.world_connection=connection;
    result.handoff_deadline=0;result.disconnect=disconnect;result.world_reservation=reservation;
    world_reservations_.emplace(key,WorldReservationEntry{reservation,{},{},false});
    session.location=LoginSession::Location::world;
    session.world_connection=connection;
    session.handoff_deadline=0;
    session.disconnect=std::move(disconnect);
    return {std::move(result),false};
}

bool Registry::world_reserved(std::uint32_t user_id,std::uint8_t realm) const {
    std::lock_guard lock(mutex_);
    const auto found=world_reservations_.find({realm,user_id});
    return found!=world_reservations_.end() && !found->second.active.expired();
}

bool Registry::owns_world_reservation(const std::shared_ptr<const WorldReservation>& reservation) const {
    if(!reservation) return false;
    std::lock_guard lock(mutex_);
    const auto found=world_reservations_.find({reservation->realm(),reservation->account()});
    return found!=world_reservations_.end() && found->second.active.lock()==reservation;
}

std::shared_ptr<const WorldAdoption> Registry::adopt_world(const std::shared_ptr<const WorldReservation>& reservation) {
    if(!reservation) return {};
    std::lock_guard lock(mutex_);
    const auto found=world_reservations_.find({reservation->realm(),reservation->account()});
    if(found==world_reservations_.end() || found->second.active.lock()!=reservation ||
       found->second.failed || found->second.adoption || found->second.adoption_spent) return {};
    auto adoption=std::shared_ptr<const WorldAdoption>(new WorldAdoption(reservation));
    found->second.adoption=adoption;found->second.adoption_spent=true;return adoption;
}

bool Registry::release_world_adoption(const std::shared_ptr<const WorldAdoption>& adoption) {
    if(!adoption) return false;
    const auto& reservation=adoption->reservation();
    std::lock_guard lock(mutex_);
    const auto found=world_reservations_.find({reservation->realm(),reservation->account()});
    if(found==world_reservations_.end() || found->second.adoption!=adoption) return false;
    found->second.adoption.reset();return true;
}

std::vector<std::shared_ptr<const WorldAdoption>> Registry::world_adoptions() const {
    std::lock_guard lock(mutex_);
    std::vector<std::shared_ptr<const WorldAdoption>> result;
    for(const auto& [key,entry]:world_reservations_) if(entry.adoption) result.push_back(entry.adoption);
    return result;
}

bool Registry::retain_failed_world(const std::shared_ptr<const WorldReservation>& reservation) {
    if(!reservation) return false;
    std::lock_guard lock(mutex_);
    const auto found=world_reservations_.find({reservation->realm(),reservation->account()});
    if(found==world_reservations_.end() || found->second.active.lock()!=reservation) return false;
    found->second.failed=reservation;return true;
}

std::vector<std::shared_ptr<const WorldReservation>> Registry::failed_world_reservations() const {
    std::lock_guard lock(mutex_);
    std::vector<std::shared_ptr<const WorldReservation>> result;
    for(const auto& [key,entry]:world_reservations_) if(entry.failed) result.push_back(entry.failed);
    return result;
}

bool Registry::release_failed_world(const std::shared_ptr<const WorldReservation>& reservation) {
    if(!reservation) return false;
    std::lock_guard lock(mutex_);
    const auto found=world_reservations_.find({reservation->realm(),reservation->account()});
    if(found==world_reservations_.end() || found->second.failed!=reservation) return false;
    found->second.failed.reset();return true;
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
    // Copy every returned owner before removing any. A throwing callback copy
    // or allocation must not erase earlier owners without returning them.
    for(const auto& [id,session]:sessions_) {
        (void)id;
        if(session.location==LoginSession::Location::handoff && session.handoff_deadline<=now_ms)
            expired.push_back(session);
    }
    for(const auto& session:expired)sessions_.erase(session.user_id);
    return expired;
}

std::vector<Registry::ExpiredHandoff> Registry::expire_with_disconnect_fences(std::uint64_t now_ms) {
    std::lock_guard lock(mutex_);
    std::vector<ExpiredHandoff> expired;
    // Stage all throwing copies and capabilities before removing any owner.
    for(const auto& [id,owner]:sessions_) {
        if(owner.location!=LoginSession::Location::handoff || owner.handoff_deadline>now_ms)continue;
        if(login_disconnect_fences_.contains(id))continue;
        expired.push_back(ExpiredHandoff{owner,{}});
        if(owner.disconnect)expired.back().disconnect_fence=std::shared_ptr<const LoginDisconnectFence>(new LoginDisconnectFence(owner));
    }
    // Roll back only capabilities staged by this sweep if map insertion fails.
    try {
        for(const auto& entry:expired)if(entry.disconnect_fence)
            login_disconnect_fences_.emplace(entry.owner.user_id,entry.disconnect_fence);
    } catch(...) {
        for(const auto& entry:expired)if(entry.disconnect_fence) {
            const auto found=login_disconnect_fences_.find(entry.owner.user_id);
            if(found!=login_disconnect_fences_.end() && found->second==entry.disconnect_fence)login_disconnect_fences_.erase(found);
        }
        throw;
    }
    for(const auto& entry:expired)sessions_.erase(entry.owner.user_id);
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
