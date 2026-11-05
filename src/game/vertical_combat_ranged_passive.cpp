#include "eternity/game/vertical_combat_ranged_passive.hpp"
#include <stdexcept>
namespace eternity::game {
std::int32_t VerticalCombatActorRangedPassive::ranged_penalty_suppression(const Actor& actor){
 if(&actor!=&actor_)throw std::invalid_argument("Ranged passive bound to another actor");
 return actor.status().skills.cross_road;
}
}
