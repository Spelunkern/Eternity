#include "eternity/game/vertical_combat_target.hpp"
namespace eternity::game {
void vertical_combat_set_target(MobTargetingZone& zone,MobRuntime& mob,Actor& actor){
 MobTargetingActorUser user(actor);
 zone.set_target(mob,user,true);
}
void VerticalCombatTargetNormalWorld::set_target(MobRuntime& mob,Actor& actor){vertical_combat_set_target(targeting_,mob,actor);}
void VerticalCombatTargetSkillServices::set_target(MobRuntime& mob,Actor& actor){vertical_combat_set_target(targeting_,mob,actor);}
}
