#pragma once
#include "eternity/game/mob_applied_skills_damage.hpp"

namespace eternity::game {
// Explicit stSkillInfo +0x6e/+0x70 projection. These are damage words, not TimeHeal.
struct MobAppliedSkillTimeDamageDefinition {
    std::uint8_t type=0;
    std::array<std::uint16_t,3> damage{};
};
struct MobAppliedSkillTimeDamageResources {
    std::uint32_t hp=0,maximum_hp=0;
    std::uint16_t sp=0,mp=0,maximum_sp=0,maximum_mp=0;
};
MobAppliedSkillTimeDamageResources project_mob_applied_skill_time_damage(const MobRuntime&,const MobRuntimeTemplate&) noexcept;
// Complete CMob::TimeDamage 004a3be0, normal original CRT precision (53 bits,
// round-to-nearest). Preserve intermediate binary32/binary64 stores and signed
// low-DWORD conversions. No clock, combat, world or allocation provider is needed.
MobAppliedSkillDamage calculate_mob_applied_skill_time_damage(const MobAppliedSkillTimeDamageDefinition&,
    const MobAppliedSkillTimeDamageResources&,std::uint32_t elapsed_ms) noexcept;
}
