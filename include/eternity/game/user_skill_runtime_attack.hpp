#pragma once
#include "eternity/game/user_skill_runtime.hpp"
namespace eternity::game {
enum class UserSkillAction {attack,heal,debuff,buff,field,once,attack_select,attack_range,debuff_select,debuff_range,debuff_around};
class UserSkillAttackServices {
public:
    virtual ~UserSkillAttackServices()=default;
    virtual void user_effect(UserSkillAction,Actor&,Actor*,const UserSkillDefinition&)=0;
    // debuff_around has no target argument in the original: nullptr is supplied.
    virtual void mob_effect(UserSkillAction,Actor&,MobRuntime*,const UserSkillDefinition&)=0;
    // Actual SendUseSPMP 00481ca0 owner, not an availability test/no-op sink.
    virtual void spend(Actor&,std::uint16_t sp,std::uint16_t mp)=0;
    virtual std::uint32_t tick_ms()=0;
};
// Resolved-skill AttackMob 00450cf0 / AttackUser 00451c80. Eligibility and cast
// preparation belong to callers. Actual effects/cost spending remain required.
void user_skill_runtime_attack_mob(UserSkillRuntime&,UserSkillHandle,std::uint32_t,MobRuntime*,UserSkillAttackServices&);
void user_skill_runtime_attack_user(UserSkillRuntime&,UserSkillHandle,std::uint32_t,Actor*,UserSkillAttackServices&);
// Concrete casting scheduler adapter. UseSkill preparation and remaining actual
// world providers remain abstract; no default normal/skill action providers.
class UserSkillRuntimeAttackWorld:public UserSkillRuntimeWorld {
public:
    UserSkillRuntimeAttackWorld(UserSkillRuntime& owner,UserSkillAttackServices& services):UserSkillRuntimeWorld(owner),owner_(owner),services_(services){}
    void attack_runtime_mob(Actor&,MobRuntime*,UserSkillHandle,std::uint32_t) final;
    void attack_runtime_user(Actor&,Actor&,UserSkillHandle,std::uint32_t) final;
private:
    UserSkillRuntime& owner_;UserSkillAttackServices& services_;
};
}
