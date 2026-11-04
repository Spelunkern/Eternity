#pragma once
#include "eternity/game/mob_applied_skills.hpp"

namespace eternity::game {
// CMob::RemApplySkill 004a0da0 is required, not an optional notification.
// It can cause death, which clears the remaining active list recursively.
class MobAppliedSkillRemovalServices {
public:
    virtual ~MobAppliedSkillRemovalServices()=default;
    virtual void remove_effects(MobAppliedSkill&) noexcept=0;
};
}
