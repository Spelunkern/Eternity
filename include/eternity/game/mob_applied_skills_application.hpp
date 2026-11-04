#pragma once
#include "eternity/game/mob_applied_skills_effects.hpp"
#include <string>

namespace eternity::game {
// Stable mob-definition projection, not the user skill catalog or a temporary
// conversion. Its definition/name storage must outlive active and pooled records.
struct MobAppliedSkillApplication {
    const SkillDefinition& definition;
    const std::string& name;
    std::uint32_t attacker_type=0,attacker_id=0;
};
class MobAppliedSkillApplicationServices {
public:
    virtual ~MobAppliedSkillApplicationServices()=default;
    virtual std::uint32_t tick() noexcept=0;
    virtual void added(const MobAppliedSkill&,std::uint8_t action) noexcept=0;
};
// CMob::AddApplySkillDebuff 004a0ca0. Vision fix uses the captured attacker ID.
void apply_mob_applied_skill_debuff(MobRuntime&,const SkillDefinition&,std::uint32_t attacker_id,
                                    MobAppliedSkillEffectServices&) noexcept;
}
