#pragma once
#include "eternity/game/user_skill_runtime.hpp"
namespace eternity::game {
class UserSkillPersistWorld {
public:
    virtual ~UserSkillPersistWorld()=default;
    virtual std::uint32_t tick_ms()=0;
    virtual void send_database(const net::Packet&)=0;
};
// Learned phase of SendDBAgentCharGetInfo 0046cab9..0046cb63. Invoke AFTER
// resources/position/equipment quality phases. Return the captured tick for the
// existing later effect/quest phases; do not sample another clock for them.
// Initialized caller scratch preserves original untouched null-slot rows.
std::uint32_t user_skill_runtime_persist(UserSkillRuntime&,UserSkillPersistWorld&,
    std::span<std::uint8_t,775> scratch);
}
