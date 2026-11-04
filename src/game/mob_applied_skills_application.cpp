#include "eternity/game/mob_applied_skills_application.hpp"
#include "eternity/game/zone_mobs.hpp"
#include <algorithm>
#include <bit>
#include <stdexcept>

namespace eternity::game {
namespace {
void increment(std::int32_t& value) noexcept {value=std::bit_cast<std::int32_t>(std::bit_cast<std::uint32_t>(value)+1U);}
void vision_fix(MobRuntime& mob,std::uint32_t id) noexcept {
    if(!id)return;
    mob.vision_fixed_user=id;
    if(mob_status(mob)!=2){mob.status=1;mob.next_move=0;}
    mob.target_kind=1;mob.target_id=id;
}
bool logged(const MobRuntime& mob) noexcept {return mob.lua_ai!=0 || (mob.respawn && mob.respawn->both);}
}
void apply_mob_applied_skill_debuff(MobRuntime& mob,const SkillDefinition& definition,std::uint32_t id,
                                    MobAppliedSkillEffectServices& services) noexcept {
    switch(definition.detail) {
    case 0x3e:case 0x43:
        for(const auto& ability:definition.abilities)
            set_mob_applied_skill_ability(mob,ability.type,-static_cast<std::int32_t>(ability.value),services);
        return;
    case 0x40:increment(mob.stun);services.send_speed(mob);return;
    case 0x41:increment(mob.stop);services.send_speed(mob);return;
    case 0x42:increment(mob.sleep);services.send_speed(mob);return;
    case 0x44:mob.attribute=0;return;
    case 0x48:vision_fix(mob,id);return;
    default:return;
    }
}
// ps_game.exe CMob::AddApplySkill 004a0aa0. Refresh leaves definition/order/effects
// unchanged; replacement detaches, reverses old effects, then reuses the same ID.
void MobAppliedSkills::apply(MobRuntime& mob,const MobAppliedSkillApplication& application,
    std::uint8_t base_attribute,std::atomic<std::uint32_t>& counter,
    MobAppliedSkillApplicationServices& services,MobAppliedSkillEffectServices& effects) {
    const auto& definition=application.definition;
    if(definition.effect!=3)return;
    std::lock_guard lock(mutex_);
    MobAppliedSkill* selected=nullptr;
    cursor_=records_.empty()?nullptr:records_.front();
    while(cursor_) {
        auto* skill=cursor_;
        if(skill->skill_id==definition.id) {
            if(skill->level==definition.level) {
                skill->level=definition.level;skill->target_type=application.attacker_type;skill->target_id=application.attacker_id;
                const auto now=services.tick();skill->previous_tick=now;skill->end_tick=now+definition.keep_time_seconds*1000U;
                if(definition.detail==0x48)vision_fix(mob,application.attacker_id);
                return;
            }
            if(definition.level<=3) {
                if(static_cast<std::int32_t>(definition.level)<=std::bit_cast<std::int32_t>(skill->level))return;
                if(logged(mob))log_.removed(*skill,6);
                const auto current=std::find(records_.begin(),records_.end(),cursor_);
                cursor_=current==records_.begin()?nullptr:*(current-1);records_.erase(current);
                if(!skill->definition)throw std::logic_error("Missing replaced skill definition");
                remove_mob_applied_skill_effects(mob,*skill,*skill->definition,base_attribute,effects);
                selected=skill;break;
            }
        }
        const auto current=std::find(records_.begin(),records_.end(),cursor_);
        cursor_=current+1==records_.end()?nullptr:*(current+1);
    }
    if(!selected)selected=acquire_created(counter);
    if(!selected)return;
    try {
        selected->definition=&definition;selected->name=application.name;
        selected->skill_id=definition.id;selected->level=definition.level;
        selected->target_type=application.attacker_type;selected->target_id=application.attacker_id;
        const auto now=services.tick();selected->previous_tick=now;
        selected->end_tick=now+definition.keep_time_seconds*1000U;
        apply_mob_applied_skill_debuff(mob,*selected->definition,application.attacker_id,effects);
        if(logged(mob))services.added(*selected,5);
        records_.push_back(selected);
    } catch(...) {pool_.recycle(selected);throw;}
}
}
