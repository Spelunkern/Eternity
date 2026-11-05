#pragma once
#include "eternity/game/vertical_combat_damage.hpp"
#include "eternity/game/vertical_combat_skill_tail.hpp"
#include "eternity/game/mob_targeting_actor.hpp"
namespace eternity::game {
// ps_game.exe CMob::SetTarget 0049fad0 with help enabled. The caller retains
// the actor, mob, paired zone grids, catalog and targeting world through all
// synchronous callbacks on their shared serialized executor. Targeting must use
// the same serialized RNG owner as combat, including for nested help/timers.
void vertical_combat_set_target(MobTargetingZone&,MobRuntime&,Actor&);
class VerticalCombatTargetNormalWorld:public VerticalCombatNormalDamageWorld {
public:
 VerticalCombatTargetNormalWorld(RandomSource& random,ZoneUsers& users,VerticalCombatRangedPassive& passive,std::span<const float,81> elements,MobTargetingZone& targeting)
  :VerticalCombatNormalDamageWorld(random,users,passive,elements),targeting_(targeting){}
 void set_target(MobRuntime&,Actor&) final;
private:MobTargetingZone& targeting_;
};
class VerticalCombatTargetSkillServices:public VerticalCombatSkillTailServices {
public:
 VerticalCombatTargetSkillServices(RandomSource& random,VerticalCombatSkillRuntimeDamage& damage,MobTargetingZone& targeting)
  :VerticalCombatSkillTailServices(random,damage),targeting_(targeting){}
 void set_target(MobRuntime&,Actor&) final;
private:MobTargetingZone& targeting_;
};
}
