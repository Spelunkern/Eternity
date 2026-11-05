#pragma once
#include "eternity/game/vertical_combat_skill_primary.hpp"
namespace eternity::game {
// CUser::AttackMob 0044fd90 continuation after the primary result packet.
// All borrowed owners and damage arrays survive the synchronous callbacks.
class VerticalCombatSkillTailWorld {
public:
 virtual ~VerticalCombatSkillTailWorld()=default;
 // Pure read of captured stSkillInfo +76/+78/+7a. These bytes must remain
 // stable through this read; absence of their actual owner is an error.
 virtual std::array<std::uint16_t,3> secondary_words(const UserSkillDefinition&)=0;
 // Actual DamageParty / CMob::Damage / stop / death. Party damage uses the
 // live primary HP value, while Damage receives the live secondary triple.
 virtual bool apply_secondary(Actor&,MobRuntime&,const UserSkillDefinition&,std::array<std::int32_t,3>& primary,std::array<std::int32_t,3>& secondary)=0;
 // First branch calls PSendViewCombat 004bd8d0 even without a zone; the other
 // calls PSendView 00421220 only when a zone exists. Both use priority 5.
 virtual void send_secondary(Actor&,bool first,const net::Packet&)=0;
 virtual void set_target(MobRuntime&,Actor&)=0; // 0049fad0, help=true
 // Actual CMob::AddApplySkill 004a0aa0: retained definition, kind 1, actor ID.
 virtual void add_mob_skill(MobRuntime&,Actor&,const UserSkillDefinition&)=0;
 virtual void attack_item_option(Actor&,MobRuntime&)=0; // 0044c720
};
void vertical_combat_skill_tail(Actor&,MobRuntime&,const UserSkillDefinition&,bool first,bool second,bool killed,bool drain,std::array<std::int32_t,3>& primary,VerticalCombatSkillTailWorld&);
class VerticalCombatSkillTailServices:public VerticalCombatSkillPrimaryServices,public VerticalCombatSkillTailWorld {
public:
 using VerticalCombatSkillPrimaryServices::VerticalCombatSkillPrimaryServices;
 void after_primary(Actor&,MobRuntime&,const UserSkillDefinition&,bool first,bool second,bool killed,bool drain,std::array<std::int32_t,3>&) final;
 void send_secondary(Actor&,bool first,const net::Packet&) final;
 // Actual CObjectConnection::PSendViewCombat 004bd8d0, priority 5, flag 0.
 virtual void send_secondary_combat(Actor&,const net::Packet&)=0;
};
}
