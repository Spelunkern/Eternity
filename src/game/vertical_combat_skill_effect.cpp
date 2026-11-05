#include "eternity/game/vertical_combat_skill_effect.hpp"
#include <stdexcept>
namespace eternity::game {
void vertical_combat_add_mob_skill(MobRuntime& mob,Actor& actor,const UserSkillDefinition& skill,VerticalCombatSkillSlotServices& services){
 auto& owner=mob.applied_skills();
 const auto binding=services.resolve(mob,skill);
 if(!binding||!binding->catalog||binding->catalog->skill_definition(binding->slot)!=&skill.effect)throw std::logic_error("Combat skill requires identical retained definition and catalog slot");
 if(!owner.apply(binding->slot,1,actor.id()))throw std::logic_error("Combat skill catalog slot is unavailable");
}
VerticalCombatAppliedSkillServices::VerticalCombatAppliedSkillServices(RandomSource& r,VerticalCombatSkillRuntimeDamage& d,MobTargetingZone& t,const ItemDefinitions& i,VerticalCombatView& v,VerticalCombatHitServices& h,VerticalCombatSkillSlotServices& s):VerticalCombatHitSkillServices(r,d,t,i,v,h),slots_(s){}
void VerticalCombatAppliedSkillServices::add_mob_skill(MobRuntime& mob,Actor& actor,const UserSkillDefinition& skill){vertical_combat_add_mob_skill(mob,actor,skill,slots_);}
}
