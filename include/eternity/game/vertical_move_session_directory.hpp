#pragma once
#include "eternity/game/vertical_move_session_resources.hpp"
#include "eternity/game/vertical_session_registry.hpp"
#include <map>

namespace eternity::game {
// World-strand owner for one movement resource set per registered actor.
// lease retains the actual pool and all referenced party/shop projections;
// it must not retain the session or this directory. The registry and directory
// outlive dispatch and close. Stop producers before close; extracted batches,
// socket IO, guild/social state and final saves retain their own fences.
template<class Session> class VerticalMoveSessionDirectory final {
    struct Entry {
        std::weak_ptr<Session> owner;
        Actor* actor;
        std::shared_ptr<void> lease;
        VerticalMoveSessionResources resources;
        Entry(const std::shared_ptr<Session>& session,Actor& retained_actor,std::shared_ptr<void> lifetime,PartyMembershipSendWorld& pool,
              PartyMembershipUser& connection,ZoneLeaveShopState& shop)
            :owner(session),actor(&retained_actor),lease(std::move(lifetime)),resources(pool,connection,shop) {}
    };
public:
    explicit VerticalMoveSessionDirectory(VerticalSessionRegistry<Session>& registry):registry_(registry) {}
    VerticalMoveSessionDirectory(const VerticalMoveSessionDirectory&)=delete;
    VerticalMoveSessionDirectory& operator=(const VerticalMoveSessionDirectory&)=delete;
    std::shared_ptr<VerticalMoveSessionResources> attach(const std::shared_ptr<Session>& owner,Actor& actor,
        std::shared_ptr<void> lease,PartyMembershipSendWorld& pool,PartyMembershipUser& connection,ZoneLeaveShopState& shop) {
        if(!owner || !lease || registry_.retain_actor(actor)!=owner || connection.id!=actor.id())
            throw std::invalid_argument("Movement resources require the registered actor and connection");
        auto entry=std::make_shared<Entry>(owner,actor,std::move(lease),pool,connection,shop);
        if(!entries_.emplace(actor.id(),entry).second)throw std::logic_error("Movement resources already attached");
        return {entry,&entry->resources};
    }
    std::shared_ptr<VerticalMoveSessionResources> find(Actor& actor) {
        auto it=entries_.find(actor.id());if(it==entries_.end())return {};
        auto owner=registry_.retain_actor(actor);
        if(!owner || it->second->owner.lock()!=owner)throw std::logic_error("Movement resources lost their registered actor");
        return {it->second,&it->second->resources};
    }
    // Find an attached actor by its pinned session even if that session has
    // lost its world-user projection during teardown. Registry retention must
    // still hold the same owner and actor until close completes.
    Actor* actor_for(const Session& session) {
        for(const auto& [id,entry]:entries_) {
            auto owner=entry->owner.lock();
            if(owner.get()!=&session)continue;
            if(!entry->actor || entry->actor->id()!=id || registry_.retain_actor(*entry->actor)!=owner)
                throw std::logic_error("Movement resources lost their registered actor");
            return entry->actor;
        }
        return nullptr;
    }
    std::size_t close(Actor& actor) {
        auto it=entries_.find(actor.id());if(it==entries_.end())throw std::logic_error("Movement resources are not attached");
        auto owner=registry_.retain_actor(actor);
        if(!owner || it->second->owner.lock()!=owner)throw std::logic_error("Movement resources lost their registered actor");
        const auto count=it->second->resources.close_queue();entries_.erase(it);return count;
    }
    std::size_t size() const noexcept {return entries_.size();}
private:
    VerticalSessionRegistry<Session>& registry_;std::map<std::uint32_t,std::shared_ptr<Entry>> entries_;
};
}
