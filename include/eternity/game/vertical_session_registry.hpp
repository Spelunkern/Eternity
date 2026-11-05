#pragma once
#include "eternity/game/actor.hpp"
#include "eternity/session/registry.hpp"
#include <map>
#include <limits>
#include <memory>
#include <stdexcept>
#include <asio/any_io_executor.hpp>
#include <asio/post.hpp>
#include <asio/strand.hpp>
#include <functional>
#include <exception>
#include <optional>

namespace eternity::game {
// Owns actual world-session objects across socket closure. All operations and
// destruction occur on the serialized world executor; callers pin returned
// shared pointers through any callback that can remove a registry entry.
// A session must retain its Actor and every dependency until removed/drained.
// Internal 0302 acknowledgments contain only account ID, not a generation;
// do not register a replacement until the old account's acknowledgments and
// queued work are drained. The release identity check alone cannot route them.
template<class Session> class VerticalSessionRegistry {
public:
    VerticalSessionRegistry()=default;
    VerticalSessionRegistry(const VerticalSessionRegistry&)=delete;
    VerticalSessionRegistry& operator=(const VerticalSessionRegistry&)=delete;
    VerticalSessionRegistry(VerticalSessionRegistry&&)=delete;
    VerticalSessionRegistry& operator=(VerticalSessionRegistry&&)=delete;
    // Bind the exact network registry and realm before any live owner is registered.
    // Once bound, every account must carry a matching token issued by that registry.
    // The binding retains it until all world owners and their registry drain.
    void bind_network_registry(std::shared_ptr<session::Registry> registry,std::uint8_t realm) {
        if(!registry || realm>=254 || network_registry_ || !accounts_.empty() || draining_)
            throw std::logic_error("Invalid world network registry binding");
        auto identity=std::make_shared<const std::uint8_t>(0);
        network_registry_=std::move(registry);network_realm_=realm;owner_identity_=std::move(identity);
    }
    // Opaque composition identity, unique to this bound world registry.
    // Read on the registry strand; it does not establish factory ownership.
    std::shared_ptr<const void> owner_identity() const {return owner_identity_;}
    bool matches_network_binding(const std::shared_ptr<session::Registry>& registry,std::uint8_t realm) const {
        return network_registry_ && network_registry_==registry && network_realm_==realm;
    }
    // Legacy/local owners can use this overload only while unbound. A live
    // network-bound world must not bypass its atomic account reservation.
    void reserve(std::uint32_t account,std::shared_ptr<Session> owner) {
        if(network_registry_)throw std::logic_error("Network-bound world requires a claim");
        reserve_entry(account,std::move(owner));
    }
    // Transfer a live token from the bound network registry to the owner.
    void reserve(std::uint32_t account,std::shared_ptr<Session> owner,
                 std::shared_ptr<const session::WorldReservation> reservation) {
        if(!network_registry_ || !reservation || reservation->account()!=account || reservation->realm()!=network_realm_)
            throw std::invalid_argument("Missing, foreign or mismatched world reservation");
        // The local insertion is private to this strand. Adoption is the
        // cross-registry atomic decision; on failure remove the provisional
        // entry without ever publishing an owner outside this call.
        reserve_entry(account,std::move(owner));
        try {
            auto adoption=network_registry_->adopt_world(reservation);
            if(!adoption)throw std::invalid_argument("Foreign, failed or already adopted world reservation");
            accounts_.at(account).adoption=std::move(adoption);
        } catch(...) {accounts_.erase(account);throw;}
    }
    void bind_actor(std::uint32_t account,const Session& expected,Actor& actor) {
        const auto it=accounts_.find(account);
        if(it==accounts_.end() || it->second.owner.get()!=&expected || it->second.actor || actor.account_id()!=account)
            throw std::logic_error("Invalid world actor ownership");
        if(!actors_.emplace(actor.id(),account).second)throw std::logic_error("Duplicate world actor");
        it->second.actor=&actor;
    }
    void retain(std::shared_ptr<Session> owner,Actor& actor) {
        if(!owner)throw std::invalid_argument("Missing world session owner");
        const auto account=actor.account_id();auto pin=owner;reserve(account,std::move(owner));
        try {bind_actor(account,*pin,actor);}catch(...){accounts_.erase(account);throw;}
    }
    std::shared_ptr<Session> find_account(std::uint32_t account) const {
        const auto it=accounts_.find(account);return it==accounts_.end()?std::shared_ptr<Session>{}:it->second.owner;
    }
    // This pins the registered character even after world-index removal; the
    // caller must still validate the exact retained user/actor being requested.
    std::shared_ptr<Session> find_character(std::uint32_t character) const {
        const auto id=actors_.find(character);if(id==actors_.end())return {};
        const auto owner=accounts_.find(id->second);
        return owner!=accounts_.end() && owner->second.actor && owner->second.actor->id()==character?owner->second.owner:std::shared_ptr<Session>{};
    }
    std::shared_ptr<Session> retain_actor(const Actor& actor) const {
        const auto id=actors_.find(actor.id());if(id==actors_.end())return {};
        const auto it=accounts_.find(id->second);return it!=accounts_.end() && it->second.actor==&actor?it->second.owner:std::shared_ptr<Session>{};
    }
    // Check a stored non-owning pointer without dereferencing it. The owner
    // still has to prevent address reuse while a relationship is outstanding.
    std::shared_ptr<Session> retain_actor_pointer(const Actor* actor) const {
        if(!actor)return {};
        for(const auto& [account,entry]:accounts_) {
            (void)account;if(entry.actor==actor)return entry.owner;
        }
        return {};
    }
    // Count accepted DB requests until their queued jobs AND resulting replies
    // have been processed on the world executor. Producers must stop before
    // release; unsolicited/untracked packets require a transport-level fence.
    bool begin_database_work(std::uint32_t account,const Session& expected) {
        auto it=accounts_.find(account);if(it==accounts_.end() || it->second.owner.get()!=&expected)return false;
        if(it->second.database_work==std::numeric_limits<std::size_t>::max())throw std::overflow_error("Too many database operations");
        ++it->second.database_work;return true;
    }
    bool finish_database_work(std::uint32_t account,const Session& expected) {
        auto it=accounts_.find(account);if(it==accounts_.end() || it->second.owner.get()!=&expected || !it->second.database_work)return false;
        --it->second.database_work;return true;
    }
    std::size_t pending_database_work(std::uint32_t account) const {
        const auto it=accounts_.find(account);return it==accounts_.end()?0:it->second.database_work;
    }
    // Refuse an obsolete owner or any account with tracked DB work in flight.
    bool release(std::uint32_t account,const Session& expected) {
        const auto it=accounts_.find(account);if(it==accounts_.end() || it->second.owner.get()!=&expected || it->second.database_work)return false;
        auto adoption=it->second.adoption;
        if(adoption && !network_registry_->release_world_adoption(adoption))
            throw std::logic_error("World adoption was lost before owner release");
        if(it->second.actor)actors_.erase(it->second.actor->id());accounts_.erase(it);
        notify_empty();return true;
    }
    // Call on the same world strand after stopping transport admission and all
    // producers that can create sessions. This refuses further reservations and
    // posts exactly one completion after the final actual release. Posting on
    // the strand defers notification until the releasing owner has returned
    // from release() and completed its surrounding disconnect callback.
    // It does not itself close sessions, stop DB producers, fence unsolicited
    // replies, or prove that separately owned resources have drained.
    void close_admission_and_wait(asio::strand<asio::any_io_executor> strand,std::function<void()> done) {
        if(!strand.running_in_this_thread())throw std::logic_error("Session drain requires the world strand");
        if(draining_ || !done)throw std::logic_error("Invalid session drain registration");
        drain_strand_=std::move(strand);drain_done_=std::move(done);draining_=true;
        notify_empty();
    }
    // If posting the notification failed, retry on the same running strand.
    void retry_drain_notification() {
        if(!drain_strand_ || !drain_strand_->running_in_this_thread())throw std::logic_error("Session drain requires the world strand");
        notify_empty();
    }
    std::exception_ptr drain_notification_error() const {
        if(!drain_strand_ || !drain_strand_->running_in_this_thread())throw std::logic_error("Session drain requires the world strand");
        return drain_error_;
    }
    bool admission_closed() const noexcept {return draining_;}
    std::size_t size() const noexcept {return accounts_.size();}
private:
    void reserve_entry(std::uint32_t account,std::shared_ptr<Session> owner) {
        if(draining_)throw std::logic_error("World session admission is closed");
        if(!owner || !account)throw std::invalid_argument("Missing world session owner or account");
        if(accounts_.contains(account))throw std::logic_error("Duplicate active world session");
        for(const auto& entry:accounts_)if(entry.second.owner.get()==owner.get())throw std::logic_error("Session already owns an account");
        accounts_.emplace(account,Entry{std::move(owner),nullptr});
    }
    void notify_empty() {
        if(!draining_ || !accounts_.empty() || !drain_done_)return;
        // Copy into post before clearing; allocation failure leaves a retryable
        // callback and does not report successful shutdown.
        try {
            asio::post(*drain_strand_,drain_done_);
            drain_done_={};drain_error_={};
        } catch(...) {drain_error_=std::current_exception();}
    }
    std::shared_ptr<session::Registry> network_registry_;
    std::shared_ptr<const void> owner_identity_;
    std::uint8_t network_realm_=0xff;
    bool draining_=false;
    std::optional<asio::strand<asio::any_io_executor>> drain_strand_;
    std::function<void()> drain_done_;
    std::exception_ptr drain_error_;
    struct Entry {std::shared_ptr<Session> owner;Actor* actor;std::size_t database_work=0;std::shared_ptr<const session::WorldAdoption> adoption;};
    std::map<std::uint32_t,Entry> accounts_;std::map<std::uint32_t,std::uint32_t> actors_;
};
}
