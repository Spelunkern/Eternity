#pragma once
#include "eternity/game/user_skill_runtime_attack.hpp"
namespace eternity::game {
class UserSkillStrikeWorld {
public:
    virtual ~UserSkillStrikeWorld()=default;
    // Actual SendUseSkill 0044e900: source kind4, target ID, final DWORD0.
    virtual void announce_multi(Actor&,Actor&,const UserSkillDefinition&)=0;
    // Actual SkillAttackToUser 0044f4c0; flags retain original argument order.
    virtual void hit(Actor&,Actor&,const UserSkillDefinition&,bool first,bool second)=0;
    // Actual scalar CUser::Damage 0044c120: kind1, self ID, live signed HP.
    virtual bool self_damage(Actor&,std::int32_t)=0;
    // Actual CUser::Death 004586a0: type1, self attacker ID, captured forced flag.
    virtual void self_death(Actor&,bool forced)=0;
    virtual void range(Actor&,Actor*,const UserSkillDefinition&)=0; // 00451f80
    virtual void range_select(Actor&,Actor*,const UserSkillDefinition&)=0; // 00452560
};
// Actual SkillAttack 00451d90 and selected coordinator 00451dd0.
// Caller retains actor/target/definition across callbacks; no second damage owner.
void user_skill_runtime_strike(Actor&,Actor*,const UserSkillDefinition&,UserSkillStrikeWorld&);
}
