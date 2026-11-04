#pragma once
#include "eternity/game/mob_applied_skills.hpp"
#include "eternity/game/mobs.hpp"
#include "eternity/game/skills.hpp"

namespace eternity::game {
class MobAppliedSkillEffectServices {
public:
    virtual ~MobAppliedSkillEffectServices()=default;
    virtual void rebuild_attack(MobRuntime&) noexcept=0;
    virtual void send_speed(MobRuntime&) noexcept=0;
    // Required remaining RemApplySkill branches 0x3d/0x3f: final damage,
    // death, previous-attack names and client result delivery. Not a notification.
    virtual void finish_damage_skill(MobRuntime&,MobAppliedSkill&,const SkillDefinition&) noexcept=0;
};
// ps_game.exe CMob::SetSkillAbility 004a1420. Preserve each immediate callback.
void set_mob_applied_skill_ability(MobRuntime&,std::uint32_t type,std::int32_t delta,
                                   MobAppliedSkillEffectServices&) noexcept;
// CMob::RemApplySkill 004a0da0 non-damage branches. base_attribute explicitly
// projects CMob::m_pInfo->byAttrib (+0x40), not the current modified attribute.
void remove_mob_applied_skill_effects(MobRuntime&,MobAppliedSkill&,const SkillDefinition&,
                                     std::uint8_t base_attribute,MobAppliedSkillEffectServices&) noexcept;
}
