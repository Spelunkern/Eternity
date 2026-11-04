#include "eternity/game/mob_applied_skills_damage.hpp"
#include "eternity/game/zone_mobs.hpp"
#include <algorithm>
#include <stdexcept>

namespace eternity::game {
// ps_game.exe CMob::ChkKeepSkill 004a10d0. No initial status/deadline guard.
void MobAppliedSkills::keep(std::uint32_t now,MobRuntime& mob,MobAppliedSkillDamageServices& damage,
                            MobAppliedSkillEffectServices& effects) {
    bool applied=false;
    {
        std::lock_guard lock(mutex_);
        cursor_=records_.empty()?nullptr:records_.front();
        while(cursor_) {
            auto* skill=cursor_;
            if(!skill->definition)throw std::logic_error("Missing applied skill definition");
            const auto& definition=*skill->definition;
            if(definition.detail==0x3d) {
                auto values=damage.time_damage(mob,definition,now-skill->previous_tick);
                skill->previous_tick=now;
                deliver_mob_applied_skill_periodic_damage(mob,*skill,values,damage);
                applied=true;
            }
            if(mob_status(mob)==2)break;
            if(!cursor_)cursor_=records_.empty()?nullptr:records_.front();
            else {
                const auto previous=std::find(records_.begin(),records_.end(),cursor_);
                cursor_=previous+1==records_.end()?nullptr:*(previous+1);
            }
        }
    }
    if(applied && mob_status(mob)!=2)remove_sleep(mob,effects);
}
// CMob::RemApplySkillSleep 004a16c0: bypass per-skill effect reversal, recycle
// all sleep nodes, force the counter to zero and send speed once while locked.
void MobAppliedSkills::remove_sleep(MobRuntime& mob,MobAppliedSkillEffectServices& effects) {
    if(mob.sleep==0)return;
    std::lock_guard lock(mutex_);
    cursor_=records_.empty()?nullptr:records_.front();
    while(cursor_) {
        auto* skill=cursor_;
        if(!skill->definition)throw std::logic_error("Missing applied skill definition");
        if(skill->definition->detail==0x42) {
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
    mob.sleep=0;
    effects.send_speed(mob);
}
}
