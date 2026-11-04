#pragma once
#include "eternity/game/user_skill_runtime.hpp"
namespace eternity::game {
class UserSkillReadyWorld {
public:
    virtual ~UserSkillReadyWorld()=default;
    virtual std::uint32_t tick_ms()=0;
    virtual bool in_user_range(Actor&,Actor&,std::uint8_t)=0;
    virtual bool in_mob_range(Actor&,MobRuntime&,std::uint8_t)=0;
};
// ONLY the suffix after successful cost/equipment validation, EnableUseSkillUser
// 0044ece3 / EnableUseSkillMob 0044ef5d. Caller must execute the real prefix and
// pass its originally captured definition; these functions do not bypass it.
// Owner/runtime/definition/targets/scene/world remain retained through callbacks.
bool user_skill_runtime_ready_user_after_equipment(UserSkillRuntime&,UserSkillHandle,
    const UserSkillDefinition&,std::uint32_t caller_tick,Actor*,UserSkillReadyWorld&);
bool user_skill_runtime_ready_mob_after_equipment(UserSkillRuntime&,UserSkillHandle,
    const UserSkillDefinition&,std::uint32_t caller_tick,MobRuntime*,UserSkillReadyWorld&);
}
