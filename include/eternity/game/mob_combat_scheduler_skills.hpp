#pragma once
#include "eternity/game/mob_combat_scheduler.hpp"
#include <string_view>
namespace eternity::game {
// Non-owning projection of the selected user CSkill/stSkillInfo. This is not
// the mob-AI catalog or another skill owner. Backing objects/names stay valid
// through callbacks; selected_skill reads the live +1444/+ac4 owner each time.
struct MobCombatSchedulerSkillBinding {std::uint16_t target_type=0;std::string_view name;};
class MobCombatSchedulerSkillWorld:public MobCombatSchedulerWorld {
public:
    virtual const MobCombatSchedulerSkillBinding* selected_skill(const Actor&) const=0;
    virtual Actor* enable_skill_user(Actor&,std::uint32_t,std::uint16_t target_type)=0;
    virtual void use_skill_mob(Actor&,MobRuntime*,const MobCombatSchedulerSkillBinding&,std::uint32_t)=0;
    virtual void attack_skill_mob(Actor&,MobRuntime*,const MobCombatSchedulerSkillBinding&,std::uint32_t)=0;
    virtual void use_skill_user(Actor&,Actor&,const MobCombatSchedulerSkillBinding&,std::uint32_t)=0;
    virtual void attack_skill_user(Actor&,Actor&,const MobCombatSchedulerSkillBinding&,std::uint32_t)=0;
    virtual bool normal_user_attack(Actor&,std::uint32_t)=0;
    bool remainder(Actor&,std::uint32_t,MobCombatSchedulerRemainder) final;
};
}
