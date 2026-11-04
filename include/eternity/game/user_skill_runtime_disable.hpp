#pragma once
#include "eternity/game/actor.hpp"
namespace eternity::game {
// ChkKeepSkill 00487750..00487795 tail, AFTER actual periodic applied-effect
// traversal and lock release. The required damage flag is that traversal's result,
// not an invented default. Expiry uses ActorScene's fresh clock, not caller tick.
// Retain Actor/scene/effect definitions/pools through nested delivery callbacks.
void user_skill_runtime_keep_tail(Actor&,bool periodic_damage_seen);
}
