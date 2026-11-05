#pragma once
#include "eternity/game/world_users.hpp"
#include "eternity/game/zone_entry.hpp"

namespace eternity::game {
struct ZoneActorLeaveServices {
    // Full CUser leave-zone lifecycle 0044aee0 (trade, duel and other owners).
    std::function<void(Actor&)> user_leave_lifecycle;
    // CZone::SendView 004208c0, including real recipient/transport behavior.
    std::function<void(CellCoordinates,const net::Packet&)> send_view;
};
// CZone::LeaveUser 004161b0. The world/zone executor and the original recursive
// zone lock serialize the whole operation. User, zone and services survive
// callbacks; callbacks must not throw or reenter this subject's membership.
// Requires canonical entry state maintained after movement. Lifecycle and view
// are mandatory actual services, not substitutes for their full effects.
inline bool leave_zone_actor(RuntimeZone& zone,WorldUser& user,ZoneEntryState& state,const ZoneActorLeaveServices& services) {
    if(!services.user_leave_lifecycle || !services.send_view)throw std::invalid_argument("Incomplete zone leave services");
    if(!user.admission.zone)return false;
    if(!zone.users || user.admission.zone!=&zone || state.character_id!=user.actor.id() || !state.bound)
        throw std::logic_error("Zone leave owner mismatch");
    const auto cell=zone.users->cell(user.actor.id());
    if(!cell || cell->x!=state.cell.x || cell->z!=state.cell.z)throw std::logic_error("Zone leave cell mismatch");
    if(!zone.users->detach_for_zone_leave(user.actor.id()))throw std::logic_error("Zone leave lost user");
    services.user_leave_lifecycle(user.actor);
    net::Packet packet{2,2};for(unsigned i=0;i<4;++i)packet.push_back(static_cast<std::uint8_t>(user.actor.id()>>(i*8)));
    services.send_view(state.cell,packet);
    user.admission.zone=nullptr;state.cell={};state.bound=false;user.actor.status().in_zone=false;
    return true;
}
}
