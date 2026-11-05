#pragma once
#include "eternity/game/world_native_send_services.hpp"
#include "eternity/game/vertical_session_registry.hpp"
#include <map>
#include <set>

namespace eternity::game {
// Session supplies admission(), world_user() and channel(). Resources supplies
// pool(), connection() and queue(), as in VerticalMoveSessionResources. The
// supplied resource lookup must return the existing session resources without
// changing membership. Registry, buffers and pool outlive this adapter.
template<class Session,class Resources> class WorldRetainedSendPorts final:public WorldNativeSendPorts {
public:
    using Lookup=std::function<std::shared_ptr<Resources>(Session&)>;
    using Capacity=std::function<bool(Session&)>;
    WorldRetainedSendPorts(VerticalSessionRegistry<Session>& registry,UserViewSharedBuffers& buffers,
        PartyMembershipSendWorld& pool,Lookup lookup,Capacity capacity)
        :registry_(registry),buffers_(buffers),pool_(pool),lookup_(std::move(lookup)),capacity_(std::move(capacity)) {
        if(!lookup_ || !capacity_)throw std::invalid_argument("Retained world send requires actual resources and capacity owner");
    }
    std::shared_ptr<void> retain_actor(Actor& actor) override{return pin_actor(actor);}
    net::WorldChannel& channel(Actor& actor) override{return pin_actor(actor)->session->channel();}
    UserViewSharedBuffers& buffers() override{return buffers_;}
    PartyMembershipUser& connection(Actor& actor) override{return pin_actor(actor)->resources->connection();}
    UserPriorityQueue& priority_queue(Actor& actor) override{return pin_actor(actor)->resources->queue();}
    PartyMembershipSendWorld& priority_pool() override{return pool_;}
    bool can_submit_priority(Actor& actor) override{return capacity_(*pin_actor(actor)->session);}
    std::shared_ptr<void> retain_waiting(WorldAdmissionUser& user) override{return pin_waiting(user);}
    PartyMembershipUser& waiting_connection(WorldAdmissionUser& user) override{return pin_waiting(user)->resources->connection();}
    UserPriorityQueue& waiting_priority_queue(WorldAdmissionUser& user) override{return pin_waiting(user)->resources->queue();}
    net::WorldChannel& waiting_channel(WorldAdmissionUser& user) override{return pin_waiting(user)->session->channel();}
    bool can_submit_waiting_priority(WorldAdmissionUser& user) override{return capacity_(*pin_waiting(user)->session);}
private:
    struct Pin {std::shared_ptr<Session> session;std::shared_ptr<Resources> resources;};
    std::shared_ptr<Pin> pin_actor(Actor& actor) {
        auto session=registry_.retain_actor(actor);
        if(!session || !session->world_user() || &session->world_user()->actor!=&actor)throw std::logic_error("World send actor lost its session");
        if(const auto found=active_.find(&actor);found!=active_.end()) {
            if(auto pin=found->second.lock()) {
                if(pin->session!=session || &pin->resources->pool()!=&pool_ || pin->resources->connection().id!=actor.id())throw std::logic_error("World send actor changed pinned resources");
                return pin;
            }
            active_.erase(found);
        }
        for(auto it=active_.begin();it!=active_.end();)if(it->second.expired())it=active_.erase(it);else ++it;
        const auto [entry,inserted]=acquiring_.insert(&actor);
        if(!inserted)throw std::logic_error("World send resources are already being acquired");
        struct Guard {std::set<Actor*>& values;std::set<Actor*>::iterator entry;~Guard(){values.erase(entry);}} guard{acquiring_,entry};
        auto resources=lookup_(*session);
        if(!resources || &resources->pool()!=&pool_ || resources->connection().id!=actor.id() || registry_.retain_actor(actor)!=session || !session->world_user() || &session->world_user()->actor!=&actor)
            throw std::logic_error("World send actor lost its retained resources");
        auto pin=std::make_shared<Pin>(Pin{std::move(session),std::move(resources)});active_[&actor]=pin;return pin;
    }
    std::shared_ptr<Pin> pin_waiting(WorldAdmissionUser& user) {
        auto session=registry_.find_account(user.user_id);
        if(!session || &session->admission()!=&user || !session->world_user())throw std::logic_error("World send waiter lost its session");
        auto& actor=session->world_user()->actor;auto pin=pin_actor(actor);
        if(pin->session!=session || &session->admission()!=&user)throw std::logic_error("World send waiter changed session");
        return pin;
    }
    VerticalSessionRegistry<Session>& registry_;UserViewSharedBuffers& buffers_;PartyMembershipSendWorld& pool_;
    Lookup lookup_;Capacity capacity_;
    std::map<Actor*,std::weak_ptr<Pin>> active_;std::set<Actor*> acquiring_;
};
}
