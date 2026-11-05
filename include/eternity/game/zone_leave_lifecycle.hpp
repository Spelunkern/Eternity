#pragma once
#include "eternity/game/zone_leave_bound.hpp"
#include "eternity/game/zone_leave_duel.hpp"
#include "eternity/game/zone_leave_guild_duel.hpp"
#include "eternity/game/zone_leave_guild_creation.hpp"

namespace eternity::game {
// CUser+147c/+1480; exact semantics outside LeaveZone are not yet reconstructed.
struct ZoneLeaveTailState {std::uint32_t pending_147c=0,associated_1480=0;};
// Last scalar updates of CUser::LeaveZone 0044b1eb..0044b21f. A zero pending
// field preserves the associated value, and only vehicle mode 1 is reset.
inline void zone_leave_tail(Actor& actor,ZoneLeaveTailState& state,const std::function<void()>& leave_duels) {
    if(!leave_duels)throw std::invalid_argument("Zone leave needs duel lifecycle");
    if(actor.appearance().vehicle_mode==1){actor.actions().vehicle_request=0;actor.appearance().vehicle_mode=0;}
    if(state.pending_147c){state.pending_147c=0;state.associated_1480=0;}
    leave_duels();
}
// Full recovered ordering of CUser::LeaveZone 0044aee0..0044b224. Providers
// bind canonical session/creation/duel state and outlive this call. The caller
// owns serialization and must supply real manager/transport/zone services.
inline void zone_leave_user_lifecycle(Actor& actor,ZoneLeaveBound& bound,VerticalNpcInteraction& npc,
    PartyRequestState& party,ZoneLeaveRequestState& requests,ZoneGuildCreationServices& creation,
    std::array<std::uint8_t,98> creation_scratch,ZoneLeaveTailState& tail,DuelWorld& duel,
    ZoneLeaveGuildDuelServices& guild,const std::function<std::shared_ptr<void>(Actor&)>& retain,
    std::array<std::uint8_t,100> duel_scratch) {
    bound.prefix(npc);
    bound.requests(party,requests);
    zone_leave_guild_creation(actor,creation,creation_scratch);
    zone_leave_tail(actor,tail,[&]{zone_leave_individual_duel(actor,duel,retain,duel_scratch);zone_leave_guild_duel(actor,guild);});
}
}
