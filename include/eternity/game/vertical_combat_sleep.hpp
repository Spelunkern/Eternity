#pragma once
#include "eternity/game/vertical_combat_target.hpp"
#include "eternity/game/mob_applied_skills_effects.hpp"
namespace eternity::game {
// CMob::RemApplySkillSleep 004a16c0 against the mob's retained applied-skill
// owner. A nonzero sleep counter requires its real per-mob effect services,
// including actual speed delivery. Retain all owners through callbacks.
void vertical_combat_remove_sleep(MobRuntime&,MobAppliedSkillEffectServices&);
// sleep_effect_services is a pure, non-reentrant lookup of that retained owner;
// it must not mutate the mob or dispatch callbacks.
class VerticalCombatSleepNormalWorld:public VerticalCombatTargetNormalWorld {
public:
 using VerticalCombatTargetNormalWorld::VerticalCombatTargetNormalWorld;
 void remove_sleep(MobRuntime&) final;
 virtual MobAppliedSkillEffectServices& sleep_effect_services(MobRuntime&)=0;
};
class VerticalCombatSleepSkillServices:public VerticalCombatTargetSkillServices {
public:
 using VerticalCombatTargetSkillServices::VerticalCombatTargetSkillServices;
 void remove_sleep(MobRuntime&) final;
 virtual MobAppliedSkillEffectServices& sleep_effect_services(MobRuntime&)=0;
};
}
