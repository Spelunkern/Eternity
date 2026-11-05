#include "eternity/game/vertical_combat_sleep.hpp"
#include "eternity/game/mob_applied_skills_catalog.hpp"
namespace eternity::game {
void vertical_combat_remove_sleep(MobRuntime& mob,MobAppliedSkillEffectServices& effects){
 if(mob.sleep==0)return;
 mob.applied_skills().owner().remove_sleep(mob,effects);
}
void VerticalCombatSleepNormalWorld::remove_sleep(MobRuntime& mob){
 if(mob.sleep==0)return;
 (void)mob.applied_skills(); // Fail before calling a provider for an unbound mob.
 vertical_combat_remove_sleep(mob,sleep_effect_services(mob));
}
void VerticalCombatSleepSkillServices::remove_sleep(MobRuntime& mob){
 if(mob.sleep==0)return;
 (void)mob.applied_skills();
 vertical_combat_remove_sleep(mob,sleep_effect_services(mob));
}
}
