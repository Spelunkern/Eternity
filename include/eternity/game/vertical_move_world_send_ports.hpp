#pragma once
#include "eternity/game/vertical_move_shared_world.hpp"

namespace eternity::game {
// Base is WorldNativeSendPorts and Retained is the coordinator's
// WorldRetainedSendPorts<Session,VerticalMoveSessionResources>. The world strand owns all
// calls; capacity must query the real connection worker, never a default success.
// Attach resources before closing an admitted or zone-waiting session, and keep
// the composition, registry, pool and transport alive through the send phase.
template<class Base,class Composition,class Session,class Retained>
class VerticalMoveWorldSendPorts final:public Base {
public:
    using Capacity=std::function<bool(Session&)>;
    static std::shared_ptr<VerticalMoveWorldSendPorts> create(VerticalSessionRegistry<Session>& registry,
        std::shared_ptr<VerticalMoveSharedWorld<Session>> world,std::shared_ptr<Composition> movement,Capacity capacity) {
        if(!world || !movement || !capacity)throw std::invalid_argument("World send requires movement and connection capacity owners");
        return std::shared_ptr<VerticalMoveWorldSendPorts>(new VerticalMoveWorldSendPorts(registry,std::move(world),std::move(movement),std::move(capacity)));
    }
    std::shared_ptr<void> retain_actor(Actor& actor) override {
        auto owner=actor_owner(actor);return movement_->retain_send_visit(*owner,retained_->retain_actor(actor));
    }
    net::WorldChannel& channel(Actor& actor) override {return retained_->channel(actor);}
    UserViewSharedBuffers& buffers() override {return retained_->buffers();}
    PartyMembershipUser& connection(Actor& actor) override {return retained_->connection(actor);}
    UserPriorityQueue& priority_queue(Actor& actor) override {return retained_->priority_queue(actor);}
    PartyMembershipSendWorld& priority_pool() override {return retained_->priority_pool();}
    bool can_submit_priority(Actor& actor) override {return retained_->can_submit_priority(actor);}
    std::shared_ptr<void> retain_waiting(WorldAdmissionUser& user) override {
        auto owner=waiting_owner(user);return movement_->retain_send_visit(*owner,retained_->retain_waiting(user));
    }
    PartyMembershipUser& waiting_connection(WorldAdmissionUser& user) override {(void)waiting_owner(user);return retained_->waiting_connection(user);}
    UserPriorityQueue& waiting_priority_queue(WorldAdmissionUser& user) override {(void)waiting_owner(user);return retained_->waiting_priority_queue(user);}
    net::WorldChannel& waiting_channel(WorldAdmissionUser& user) override {(void)waiting_owner(user);return retained_->waiting_channel(user);}
    bool can_submit_waiting_priority(WorldAdmissionUser& user) override {(void)waiting_owner(user);return retained_->can_submit_waiting_priority(user);}
    std::shared_ptr<Session> actor_session(Actor& actor) {return actor_owner(actor);}
    std::shared_ptr<Session> waiting_session(WorldAdmissionUser& user) {return waiting_owner(user);}
    void with_priority_flush(Session& owner,const std::function<void()>& flush) {movement_->with_priority_flush(owner,flush);}
private:
    VerticalMoveWorldSendPorts(VerticalSessionRegistry<Session>& registry,std::shared_ptr<VerticalMoveSharedWorld<Session>> world,
        std::shared_ptr<Composition> movement,Capacity capacity):registry_(registry),world_(std::move(world)),movement_(std::move(movement)) {
        retained_=std::make_unique<Retained>(registry_,world_->buffers(),world_->send_world(),
            [this](Session& owner){return state(owner);},std::move(capacity));
    }
    std::shared_ptr<Session> actor_owner(Actor& actor) {
        auto owner=registry_.retain_actor(actor);
        if(!owner || !owner->world_user() || &owner->world_user()->actor!=&actor)
            throw std::logic_error("World send actor lost its registered session");
        return owner;
    }
    std::shared_ptr<Session> waiting_owner(WorldAdmissionUser& user) {
        auto owner=registry_.find_account(user.user_id);
        if(!owner || &owner->admission()!=&user || user.location!=3 || !owner->world_user() ||
           registry_.retain_actor(owner->world_user()->actor)!=owner)
            throw std::logic_error("World send waiting user lost its registered session");
        return owner;
    }
    std::shared_ptr<VerticalMoveSessionResources> state(Session& owner) {
        std::shared_ptr<VerticalMoveSessionResources> current;
        if(owner.closing()) {
            current=movement_->directory().find(owner.world_user()->actor);
            if(!current)throw std::logic_error("Closing world send session has no retained priority resources");
        } else current=movement_->resources(owner);
        if(&current->pool()!=&world_->send_world() || &current->connection()!=&owner.party_user())
            throw std::logic_error("World send movement resources differ from canonical owners");
        return current;
    }
    VerticalSessionRegistry<Session>& registry_;std::shared_ptr<VerticalMoveSharedWorld<Session>> world_;
    std::shared_ptr<Composition> movement_;std::unique_ptr<Retained> retained_;
};
}
