#pragma once
#include "eternity/game/zone_leave_guild_duel.hpp"

namespace eternity::game {
// CUser::PvPChkLeaveByGuildLeave 0047bd00 has the same guild-duel branches
// as PvPChkLeaveByZone 0047bafb. Call before clearing the user's guild pointer;
// services retain the actual guilds, zone peer and recursive locks.
inline void guild_leave_duel(Actor& actor,ZoneLeaveGuildDuelServices& services) {
    zone_leave_guild_duel(actor,services);
}
}
