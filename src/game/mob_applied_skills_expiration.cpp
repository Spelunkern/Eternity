#include "eternity/game/mob_applied_skills_expiration.hpp"
#include <algorithm>

namespace eternity::game {
// ps_game.exe CMob::ChkEndTimeSkill 004a12f0. The shared list cursor matters:
// RemApplySkill can invoke Death/ClearApplySkill and replace it with the sentinel.
void MobAppliedSkills::expire(std::uint32_t now,const MobAppliedSkillLogProjection& projection,
                              MobAppliedSkillRemovalServices& removal) {
    std::lock_guard lock(mutex_);
    cursor_=records_.empty()?nullptr:records_.front();
    while (cursor_) {
        auto* skill=cursor_;
        if (skill->end_tick<=now) {
            if (projection.lua_ai || (projection.respawn_present && projection.respawn_logging))
                log_.removed(*skill,6);
            const auto current=std::find(records_.begin(),records_.end(),cursor_);
            cursor_=current==records_.begin()?nullptr:*(current-1);
            records_.erase(current);
            removal.remove_effects(*skill);
            pool_.recycle(skill);
        }
        if (!cursor_) cursor_=records_.empty()?nullptr:records_.front();
        else {
            const auto previous=std::find(records_.begin(),records_.end(),cursor_);
            cursor_=previous+1==records_.end()?nullptr:*(previous+1);
        }
    }
}
}
