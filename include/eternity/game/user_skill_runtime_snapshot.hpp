#pragma once
#include "eternity/game/user_skill_runtime.hpp"
namespace eternity::game {
class UserSkillSnapshotClock {
public:
    virtual ~UserSkillSnapshotClock()=default;
    virtual std::uint32_t tick_ms()=0;
};
// SendCharacterSkill 00483540, client 0108. Required initialized caller scratch
// represents the original stack's preexisting bytes: null-slot rows are untouched.
// It is packet scratch, NOT another skill store. Retain owner/Actor/scene through
// the entire synchronous call. Tick provider runs before count/slot capture.
void user_skill_runtime_snapshot(UserSkillRuntime&,UserSkillSnapshotClock&,
    std::span<std::uint8_t,2051> scratch);
}
