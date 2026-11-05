#pragma once
#include "eternity/game/vertical_combat_skill_hit_prefix.hpp"
#include "eternity/game/vertical_combat_skill_runtime_damage.hpp"
namespace eternity::game {
// Continuation of CUser::AttackMob 0044fd90 at 0044ff47 / 00450337.
// All arguments and the mutable damage triple survive synchronous callbacks.
class VerticalCombatSkillHitDamageWorld {
public:
 virtual ~VerticalCombatSkillHitDamageWorld()=default;
 virtual bool calculate_skill_damage(Actor&,MobRuntime&,const UserSkillDefinition&,std::array<std::int32_t,3>&)=0;
 virtual void wear_weapon(Actor&)=0; // CUser::QualityByAttack 0045b260
 // Resume at 0044ff71 or 00450359. Must execute party credit, Damage,
 // death, packets and all remaining effects; do not repeat the prefix.
 virtual void finish_skill_hit(Actor&,MobRuntime&,const UserSkillDefinition&,bool first,bool second,bool critical,std::array<std::int32_t,3>&)=0;
};
void vertical_combat_skill_hit_damage(Actor&,MobRuntime&,const UserSkillDefinition&,bool first,bool second,VerticalCombatSkillHitDamageWorld&);
struct VerticalCombatSkillRawDamage {std::uint8_t mode;std::array<std::uint16_t,3> values;};
// Connects the selected casting action, real success and existing retained
// runtime damage owner. The raw skill bytes must be from the captured skill;
// all service and remaining-hit providers are mandatory.
class VerticalCombatSkillHitDamageServices:public VerticalCombatSkillHitSelectSuccessServices,public VerticalCombatSkillHitDamageWorld {
public:
 VerticalCombatSkillHitDamageServices(RandomSource& random,VerticalCombatSkillRuntimeDamage& damage);
 bool bound_to_runtime(const UserSkillRuntime& runtime) const noexcept {return damage_.bound_to(runtime);}
 void continue_hit(Actor&,MobRuntime&,const UserSkillDefinition&,bool first,bool second) final;
 bool calculate_skill_damage(Actor&,MobRuntime&,const UserSkillDefinition&,std::array<std::int32_t,3>&) final;
 virtual VerticalCombatSkillRawDamage raw_skill_damage(const UserSkillDefinition&)=0;
private:VerticalCombatSkillRuntimeDamage& damage_;
};
}
