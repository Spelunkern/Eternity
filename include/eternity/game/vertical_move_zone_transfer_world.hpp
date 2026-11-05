#pragma once
#include "eternity/game/vertical_move_zone_transfer.hpp"
#include "eternity/game/vertical_world_session_packet_router.hpp"
#include "eternity/game/vertical_quests_world_session.hpp"
#include "eternity/game/zone_actor_leave.hpp"

namespace eternity::game {
// All owners and returned callback captures are used on the world strand and
// must be retained by the transfer's zone/provider lease. The leave factory
// supplies the complete user-leave lifecycle and real source view delivery.
// prepare_destination writes the real target map/position and sends the party
// move notification after source leave and before world-wait publication.
inline VerticalMoveZoneTransfer<VerticalWorldSession>::Ports vertical_move_world_transfer_ports(
    VerticalWorldSession::Registry& registry,VerticalWorldSessionPacketRouter& router,WorldAdmission& admission,
    std::function<ZoneActorLeaveServices(VerticalWorldSession&,RuntimeZone&)> leave_services,
    std::function<void(VerticalWorldSession&,RuntimeZone&)> prepare_destination) {
    if(!leave_services || !prepare_destination)throw std::invalid_argument("Zone transfer requires real leave and destination services");
    using Ports=VerticalMoveZoneTransfer<VerticalWorldSession>::Ports;Ports ports;
    ports.suspend=[&router](VerticalWorldSession& owner){router.suspend_gameplay(owner);};
    ports.invalidate=[&router](VerticalWorldSession& owner){router.invalidate_gameplay(owner);};
    ports.resume=[&router](VerticalWorldSession& owner){router.resume_gameplay(owner);};
    ports.leave=[leave_services=std::move(leave_services)](VerticalWorldSession& owner,RuntimeZone& source){
        auto* user=owner.world_user();if(!user)throw std::logic_error("Zone transfer lost its user");
        auto services=leave_services(owner,source);
        if(!leave_zone_actor(source,*user,owner.zone_entry(),services))throw std::logic_error("Zone transfer source leave failed");
    };
    ports.queue_destination=[&admission,prepare=std::move(prepare_destination)](VerticalWorldSession& owner,RuntimeZone& destination){
        prepare(owner,destination);
        if(!destination.map || owner.admission().map!=destination.map->settings.id)
            throw std::logic_error("Zone transfer destination map differs from selected zone");
        admission.wait_after_zone_leave(owner.admission());
    };
    ports.rebind_quests=[&registry](VerticalWorldSession& owner,RuntimeZone& source,RuntimeZone& destination){
        vertical_world_quests_rebind_zone(owner,source,destination,registry);
    };
    ports.quests_bound=[&registry](VerticalWorldSession& owner,RuntimeZone& zone,VerticalNpcInteraction& interaction){
        auto* user=owner.world_user();if(!user || !zone.npcs)return false;
        VerticalWorldQuestSessions sessions(registry);auto quests=sessions.retain(user->actor);
        return quests && quests->quests().owns_npc_zone(*zone.npcs) && quests->packets().uses_interaction(interaction);
    };
    return ports;
}
}
