#pragma once
#include "eternity/game/mob_applied_skills_effects.hpp"
#include <array>

namespace eternity::game {
using MobAppliedSkillDamage=std::array<std::int32_t,3>; // HP, SP, MP; mutable Damage input/output.
using MobAppliedSkillDamagePacket=std::array<std::uint8_t,15>;
class MobAppliedSkillDamageServices {
public:
    virtual ~MobAppliedSkillDamageServices()=default;
    virtual std::uint32_t tick() noexcept=0;
    virtual MobAppliedSkillDamage time_damage(MobRuntime&,const SkillDefinition&,std::uint32_t elapsed) noexcept=0;
    virtual bool damage_periodic(MobRuntime&,std::uint32_t attacker_type,std::uint32_t attacker_id,
                                 MobAppliedSkillDamage&) noexcept=0;
    virtual bool damage_hp(MobRuntime&,std::uint32_t attacker_type,std::uint32_t attacker_id,
                           std::uint32_t captured_hp) noexcept=0;
    virtual void death(MobRuntime&,std::uint32_t attacker_type,std::uint32_t attacker_id) noexcept=0;
    // Original PSendView: current zone/position, radius 60, two zero options, priority 6.
    virtual void send_result(MobRuntime&,const MobAppliedSkillDamagePacket&,float radius,std::uint8_t priority) noexcept=0;
};
// ChkKeepSkill has already calculated damage and committed previous_tick.
void deliver_mob_applied_skill_periodic_damage(MobRuntime&,MobAppliedSkill&,MobAppliedSkillDamage&,
                                             MobAppliedSkillDamageServices&) noexcept;
// ps_game.exe CMob::RemApplySkill 004a0da0, branches 0x3d / 0x3f only.
// The subject/skill/definition survive callbacks. name projects the original
// definition's NUL-terminated 32-byte name. Other effect types use the effect dispatcher.
void finish_mob_applied_skill_damage(MobRuntime&,MobAppliedSkill&,const SkillDefinition&,
                                    MobAppliedSkillDamageServices&) noexcept;
}
