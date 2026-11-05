#pragma once
#include "eternity/game/vertical_combat_skill_hit_damage.hpp"
namespace eternity::game {
// Primary credit and packet portion of CUser::AttackMob 0044fd90. All
// references, the skill and the mutable damage triple outlive callbacks.
class VerticalCombatSkillPrimaryWorld {
public:
 virtual ~VerticalCombatSkillPrimaryWorld()=default;
 // Actual DamageParty / CMob::Damage / StatusAttackStop / CMob::Death,
 // including the live mob-owned ledger and party reload; aggro is skill.aggro.
 virtual bool apply_primary(Actor&,MobRuntime&,const UserSkillDefinition&,std::array<std::int32_t,3>&)=0;
 // Actual CZone::PSendView checks zone, radius 60, source sent immediately, priority 5.
 virtual void send_primary_view(Actor&,const net::Packet&)=0;
 // Resume at 0045017e, 00450437, or 0045053f (drain). The provider must
 // execute secondary damage, target, debuffs and item options as applicable.
 virtual void after_primary(Actor&,MobRuntime&,const UserSkillDefinition&,bool first,bool second,bool killed,bool drain,std::array<std::int32_t,3>&)=0;
};
void vertical_combat_skill_primary(Actor&,MobRuntime&,const UserSkillDefinition&,bool first,bool second,bool critical,std::array<std::int32_t,3>&,VerticalCombatSkillPrimaryWorld&);
class VerticalCombatSkillPrimaryServices:public VerticalCombatSkillHitDamageServices,public VerticalCombatSkillPrimaryWorld {
public:
 using VerticalCombatSkillHitDamageServices::VerticalCombatSkillHitDamageServices;
 void finish_skill_hit(Actor&,MobRuntime&,const UserSkillDefinition&,bool first,bool second,bool critical,std::array<std::int32_t,3>&) final;
 void send_primary_view(Actor&,const net::Packet&) final;
};
}
