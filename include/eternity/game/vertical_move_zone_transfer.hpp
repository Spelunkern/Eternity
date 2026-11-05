#pragma once
#include "eternity/game/vertical_move_session_directory.hpp"
#include <bit>

namespace eternity::game {
// One retained, serialized transfer. begin() leaves and queues the original
// world/zone wait; complete() runs only after actual destination admission.
// The lease retains both zones and all providers across the asynchronous wait.
// After suspend succeeds, no failure path resumes dispatch. The caller owns
// recovery if any required service fails or partially changes state.
template<class Session> class VerticalMoveZoneTransfer final {
public:
    struct Ports {
        std::function<void(Session&)> suspend,invalidate,resume;
        std::function<void(Session&,RuntimeZone&)> leave,queue_destination;
        std::function<void(Session&,RuntimeZone&,RuntimeZone&)> rebind_quests;
        // Must validate both the quest NPC zone and this exact interaction.
        std::function<bool(Session&,RuntimeZone&,VerticalNpcInteraction&)> quests_bound;
    };
    VerticalMoveZoneTransfer(VerticalSessionRegistry<Session>& registry,std::shared_ptr<Session> owner,
        std::shared_ptr<void> zone_lease,std::shared_ptr<VerticalMoveSessionResources> resources,Ports ports)
        :registry_(registry),owner_(std::move(owner)),lease_(std::move(zone_lease)),resources_(std::move(resources)),ports_(std::move(ports)) {
        if(!owner_ || !lease_ || !resources_ || !ports_.suspend || !ports_.invalidate || !ports_.leave || !ports_.queue_destination ||
           !ports_.rebind_quests || !ports_.quests_bound || !ports_.resume)
            throw std::invalid_argument("Zone transfer requires retained owners and all services");
    }
    void begin(RuntimeZone& source,RuntimeZone& destination) {
        if(phase_!=Phase::fresh)throw std::logic_error("Zone transfer is already used");phase_=Phase::starting;
        try {
            user_=owner_->world_user();source_=&source;destination_=&destination;
            if(!user_ || !source.map || !destination.map || !source.users || !destination.users || !source.npcs || !destination.npcs)
                throw std::logic_error("Zone transfer requires retained zones and actor");
            actor_=&user_->actor;id_=actor_->id();source_live();
            if(!ports_.quests_bound(*owner_,source,resources_->npc()))throw std::logic_error("Zone transfer lost its source quest zone or interaction");
            source_live();ports_.suspend(*owner_);source_live();ports_.invalidate(*owner_);source_live();
            ports_.leave(*owner_,source);subject();
            if(source.users->find_actor(id_) || destination.users->find_actor(id_) || resources_->npc().npc || owner_->zone_entry().bound)
                throw std::logic_error("Zone transfer source leave did not finish");
            ports_.queue_destination(*owner_,destination);subject();
            const auto location=owner_->admission().location;
            if((location!=2 && location!=3) || (location==2 && owner_->admission().zone) ||
               (location==3 && owner_->admission().zone!=&destination) || source.users->find_actor(id_) ||
               destination.users->find_actor(id_) || owner_->zone_entry().bound || resources_->npc().npc ||
               !ports_.quests_bound(*owner_,source,resources_->npc()))
                throw std::logic_error("Zone transfer did not enter a retained waiting state");
            subject();phase_=Phase::waiting;
        } catch(...) {phase_=Phase::failed;throw;}
    }
    void complete() {
        if(phase_!=Phase::waiting)throw std::logic_error("Zone transfer is not awaiting destination admission");phase_=Phase::finishing;
        try {
            destination_live();
            if(!ports_.quests_bound(*owner_,*source_,resources_->npc()))throw std::logic_error("Zone transfer lost its source quest zone or interaction");
            destination_live();ports_.rebind_quests(*owner_,*source_,*destination_);destination_live();
            if(!ports_.quests_bound(*owner_,*destination_,resources_->npc()))throw std::logic_error("Zone transfer did not rebind quest zone or interaction");
            destination_live();ports_.resume(*owner_);phase_=Phase::complete;
        } catch(...) {phase_=Phase::failed;throw;}
    }
private:
    enum class Phase {fresh,starting,waiting,finishing,complete,failed};
    void subject() {
        if(owner_->closing() || owner_->world_user()!=user_ || &user_->admission!=&owner_->admission() ||
           registry_.retain_actor(*actor_)!=owner_ || resources_->connection().id!=id_)
            throw std::logic_error("Zone transfer lost its registered session");
        if constexpr(requires(Session& session){session.party_user();})
            if(&resources_->connection()!=&owner_->party_user())throw std::logic_error("Zone transfer lost its canonical connection");
        if constexpr(requires(Session& session){session.dispatch_failed();})
            if(owner_->dispatch_failed())throw std::logic_error("Zone transfer session dispatch has failed");
    }
    bool entry_matches(RuntimeZone& zone) {
        const auto& entry=owner_->zone_entry();const auto cell=zone.users->cell(id_);const auto position=zone.users->position(id_);
        const auto equal=[](float a,float b){return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);};
        return entry.bound && entry.character_id==id_ && entry.map==zone.map->settings.id && cell && position &&
            entry.cell.x==cell->x && entry.cell.z==cell->z && equal(entry.position.x,position->x) &&
            equal(entry.position.y,position->y) && equal(entry.position.z,position->z);
    }
    void source_live() {
        subject();if(owner_->admission().location!=4 || owner_->admission().zone!=source_ ||
            source_->users->find_actor(id_)!=actor_ || (source_!=destination_ && destination_->users->find_actor(id_)) || !entry_matches(*source_) ||
            (resources_->npc().npc && resources_->npc().npc->zone!=source_->npcs.get()))
            throw std::logic_error("Zone transfer lost its source membership");
    }
    void destination_live() {
        subject();if(owner_->admission().location!=4 || owner_->admission().zone!=destination_ ||
            destination_->users->find_actor(id_)!=actor_ || (source_!=destination_ && source_->users->find_actor(id_)) || !entry_matches(*destination_) || resources_->npc().npc)
            throw std::logic_error("Zone transfer lost its destination membership or NPC cleanup");
    }
    VerticalSessionRegistry<Session>& registry_;std::shared_ptr<Session> owner_;std::shared_ptr<void> lease_;
    std::shared_ptr<VerticalMoveSessionResources> resources_;Ports ports_;WorldUser* user_=nullptr;Actor* actor_=nullptr;
    RuntimeZone* source_=nullptr;RuntimeZone* destination_=nullptr;std::uint32_t id_=0;Phase phase_=Phase::fresh;
};
}
