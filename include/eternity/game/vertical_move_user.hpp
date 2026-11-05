#pragma once
#include "eternity/game/zone_entry.hpp"
#include "eternity/game/user_view_actor_publication.hpp"

namespace eternity::game {
// 00416550's non-reentrant movement path. The world executor must serialize
// the actor, admission and zone; retain them through publication and do not
// move/detach this actor from any publication callback. A true result still
// requires PacketPC's direction, clock, motion and 0501 continuation.
inline bool vertical_move_user(Actor& actor,RuntimeZone& zone,ZoneEntryState& admission,
    std::uint32_t location,UserPosition destination,UserViewActorTransport& transport,
    const std::function<void(std::uint32_t)>& close) {
    if(!close || !zone.map || !zone.users || !admission.bound || admission.character_id!=actor.id() ||
       zone.map->settings.id<0 || zone.map->settings.id>65535 || admission.map!=zone.map->settings.id ||
       zone.users->find_actor(actor.id())!=&actor)
        throw std::logic_error("Movement requires retained zone admission and close service");
    if(location!=4)return false;
    const auto previous=zone.users->cell(actor.id());
    if(!previous || previous->x!=admission.cell.x || previous->z!=admission.cell.z)
        throw std::logic_error("Movement requires matching public and linked cell");
    const auto geometry=zone.map->definition.geometry();
    if(!geometry.safe_position(destination.x,destination.z))return false;
    const auto next=geometry.cell_after_move(admission.cell,destination.x,destination.z);
    // Position writes precede the original changed-cell bounds/close path.
    if(!zone.users->move_at_position(actor.id(),destination)) {admission.position=destination;close(9);return false;}
    admission.position=destination;
    if(next && (next->x!=previous->x || next->z!=previous->z)) {
        UserViewActorPublication publication(actor,transport);
        publish_user_view_difference(geometry.cell_count(),geometry.cell_count(),admission.cell,*next,publication);
        admission.cell=*next;
    }
    return true;
}
}
