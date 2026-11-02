#pragma once
#include "eternity/game/zone_npcs.hpp"
#include "eternity/game/random.hpp"

namespace eternity::game {
// CNpc::UpdateMove 004bbeb0. Call from the serialized world tick; the original
// idle/guard dispatcher separately enforces disable deadlines and combat states.
void update_npc_patrol(Npc&,std::uint32_t tick,RandomSource&,NpcMovementWorld&);
}
