#include "eternity/game/vertical_combat_skill_hit_damage.hpp"
#include <stdexcept>
namespace eternity::game {
void vertical_combat_skill_hit_damage(Actor& actor,MobRuntime& mob,const UserSkillDefinition& info,bool first,bool second,VerticalCombatSkillHitDamageWorld& world){
 std::array<std::int32_t,3> damage{};
 const bool critical=world.calculate_skill_damage(actor,mob,info,damage);
 if(critical)world.wear_weapon(actor);
 world.finish_skill_hit(actor,mob,info,first,second,critical,damage);
}
VerticalCombatSkillHitDamageServices::VerticalCombatSkillHitDamageServices(RandomSource& random,VerticalCombatSkillRuntimeDamage& damage)
 :VerticalCombatSkillHitSelectSuccessServices(random),damage_(damage){
 if(&damage.random_source()!=&random)throw std::invalid_argument("Skill success and damage require the same random owner");
}
void VerticalCombatSkillHitDamageServices::continue_hit(Actor& actor,MobRuntime& mob,const UserSkillDefinition& info,bool first,bool second){
 vertical_combat_skill_hit_damage(actor,mob,info,first,second,*this);
}
bool VerticalCombatSkillHitDamageServices::calculate_skill_damage(Actor& actor,MobRuntime& mob,const UserSkillDefinition& info,std::array<std::int32_t,3>& damage){
 const auto raw=raw_skill_damage(info);
 return damage_.calculate(actor,mob,info,raw.mode,raw.values,damage);
}
}
