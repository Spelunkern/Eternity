#pragma once
#include "eternity/game/user_skill_runtime.hpp"
namespace eternity::game {
// Original CRT nearest/53-bit environment. Does not alter global FP control.
// Original FIST overflow becomes INT_MIN, followed by wrapped DWORD subtraction.
std::int32_t user_skill_runtime_resource_cost(std::uint16_t,std::int32_t reduction);
// ONLY the SP/MP prefix of EnableUseSkillUser/Mob 0044eb40/0044ee50.
// No spending, equipment bypass, or full eligibility claim. Retain original
// definition and actual runtime through synchronous error delivery.
bool user_skill_runtime_check_resources(UserSkillRuntime&,UserSkillHandle,
    const UserSkillDefinition&,bool mob);
}
