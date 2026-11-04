#pragma once
#include "eternity/game/user_skill_runtime_attack.hpp"
namespace eternity::game {
class UserSkillUseServices:public UserSkillAttackServices {
public:
    // Actual complete eligibility, including retained weapon/shield middle.
    // Cost/readiness helpers alone are NOT a replacement for this provider.
    virtual bool eligible_user(UserSkillRuntime&,Actor*,UserSkillHandle,std::uint32_t)=0;
    virtual bool eligible_mob(UserSkillRuntime&,MobRuntime*,UserSkillHandle,std::uint32_t)=0;
    // Real PSendView radius60/channel5: user excludes source+target, mob source.
    virtual void cast_user(Actor&,Actor&,const UserSkillDefinition&,const net::Packet&)=0;
    virtual void cast_mob(Actor&,MobRuntime&,const UserSkillDefinition&,const net::Packet&)=0;
};
void user_skill_runtime_use_user(UserSkillRuntime&,UserSkillHandle,std::uint32_t,Actor*,UserSkillUseServices&);
void user_skill_runtime_use_mob(UserSkillRuntime&,UserSkillHandle,std::uint32_t,MobRuntime*,UserSkillUseServices&);
class UserSkillRuntimeUseWorld:public UserSkillRuntimeAttackWorld {
public:
    UserSkillRuntimeUseWorld(UserSkillRuntime& o,UserSkillUseServices& s):UserSkillRuntimeAttackWorld(o,s),owner_(o),services_(s){}
    void use_runtime_user(Actor&,Actor&,UserSkillHandle,std::uint32_t) final;
    void use_runtime_mob(Actor&,MobRuntime*,UserSkillHandle,std::uint32_t) final;
private:
    UserSkillRuntime& owner_;UserSkillUseServices& services_;
};
}
