#include "eternity/game/mob_applied_skills.hpp"
#include "eternity/game/mobs.hpp"
#include "eternity/game/skills.hpp"
#include <algorithm>
#include <stdexcept>

namespace eternity::game {
// ps_game.exe CMob::RemApplySkillVisionFix 004a1580. No target/status change,
// effect reversal or speed delivery; clear the ID only once, before list lock.
void MobAppliedSkills::remove_vision_fix(MobRuntime& mob) {
    if(mob.vision_fixed_user==0)return;
    mob.vision_fixed_user=0;
    std::lock_guard lock(mutex_);
    cursor_=records_.empty()?nullptr:records_.front();
    while(cursor_) {
        auto* skill=cursor_;
        if(!skill->definition)throw std::logic_error("Missing applied skill definition");
        if(skill->definition->detail==0x48) {
            if(mob.lua_ai!=0 || (mob.respawn && mob.respawn->both))log_.removed(*skill,6);
            const auto current=std::find(records_.begin(),records_.end(),cursor_);
            cursor_=current==records_.begin()?nullptr:*(current-1);
            records_.erase(current);
            pool_.recycle(skill);
        }
        if(!cursor_)cursor_=records_.empty()?nullptr:records_.front();
        else {
            const auto previous=std::find(records_.begin(),records_.end(),cursor_);
            cursor_=previous+1==records_.end()?nullptr:*(previous+1);
        }
    }
}
}
