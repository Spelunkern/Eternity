#pragma once
#include "eternity/game/guild_membership.hpp"
#include "eternity/game/actor.hpp"
#include <bit>

namespace eternity::game {
// ps_game.exe CUser::PacketUserDBSetInfo 0046d780, 050d branch 0046e668.
// Zero clears without reading GetTickCount; nonzero multiplication and
// addition preserve the original 32-bit wrapping, including negative hours.
inline void guild_restore_rejoin_deadline(GuildUser& user,const Actor& actor,std::int32_t hours) {
    if(hours==0){user.join_deadline=0;return;}
    user.join_deadline=actor.tick_ms()+std::bit_cast<std::uint32_t>(hours)*3600000u;
}
}
