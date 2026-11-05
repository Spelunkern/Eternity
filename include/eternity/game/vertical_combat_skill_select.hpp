#pragma once
#include "eternity/game/user_skill_runtime_attack.hpp"
namespace eternity::game {
// Required actual owners for CUser::SkillAttackSelect(CMob*) 00450e20.
// Retain actor, mob and captured skill definition throughout all callbacks.
class VerticalCombatSkillSelectWorld {
public:
    virtual ~VerticalCombatSkillSelectWorld()=default;
    // SendUseSkill 0044e900: source kind 4, mob ID, final DWORD 1.
    virtual void announce_multi(Actor&,MobRuntime&,const UserSkillDefinition&)=0;
    // Skill hit/debuff CUser::AttackMob 0044fd90; original flag order.
    virtual void hit(Actor&,MobRuntime&,const UserSkillDefinition&,bool first,bool second)=0;
    // CUser::Damage 0044c120, kind 1, self ID, live signed HP.
    virtual bool self_damage(Actor&,std::int32_t)=0;
    virtual void stop_mob_attack(MobRuntime&)=0; // CMob::StatusAttackStop 00490980
    virtual void self_death(Actor&)=0; // CUser::Death 004586a0, type 1, self ID, false
};
// The selected action only. Resolved casting and target validation are separate.
void vertical_combat_skill_select(Actor&,MobRuntime&,const UserSkillDefinition&,VerticalCombatSkillSelectWorld&);
// Supplies the resolved casting coordinator's selected mob attack action.
// All other effects, spending, timing and selected-hit owners remain mandatory.
class VerticalCombatSkillSelectServices:public UserSkillAttackServices,public VerticalCombatSkillSelectWorld {
public:
    void mob_effect(UserSkillAction,Actor&,MobRuntime*,const UserSkillDefinition&) final;
    virtual void other_mob_effect(UserSkillAction,Actor&,MobRuntime*,const UserSkillDefinition&)=0;
};
}
