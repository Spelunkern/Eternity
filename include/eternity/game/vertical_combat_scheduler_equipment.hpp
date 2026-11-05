#pragma once
#include "eternity/game/mob_combat_scheduler.hpp"
namespace eternity::game {
// CUser::UpdateAttack 0048ca50 reads the equipped weapon after removal of
// the mob transform. Actor, its inventory/pool and world must be retained and
// serialized across the whole call, including synchronous callbacks.
bool vertical_combat_scheduler_equipment(Actor&,std::uint32_t now,MobCombatSchedulerWorld&);
}
