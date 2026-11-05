#pragma once
#include "eternity/game/user_skill_runtime_use.hpp"
#include "eternity/game/vertical_combat_weapon_wear.hpp"
namespace eternity::game {
// Forward the scheduler's resolved skill actions to the retained selected-skill
// combat owner. Eligibility and cast delivery remain mandatory real providers.
// The retained action owner and its dependencies outlive all scheduler frames.
class VerticalCombatSchedulerUseServices:public UserSkillUseServices {
public:
 explicit VerticalCombatSchedulerUseServices(VerticalCombatWearSkillServices& actions):actions_(actions){}
 void user_effect(UserSkillAction,Actor&,Actor*,const UserSkillDefinition&) final;
 void mob_effect(UserSkillAction,Actor&,MobRuntime*,const UserSkillDefinition&) final;
 void spend(Actor&,std::uint16_t sp,std::uint16_t mp) final;
 std::uint32_t tick_ms() final;
private:VerticalCombatWearSkillServices& actions_;
};
// Binds UpdateAttack's normal mob call to CUser::AttackMob 0044ce10 and its
// cast paths to the existing retained UserSkillRuntime use/attack coordinators.
// Every other scheduler provider remains required, including real eligibility,
// actor/zone leases, range, PvP, transport and the live mob damage/death owner.
class VerticalCombatSchedulerWorld:public UserSkillRuntimeUseWorld {
public:
 VerticalCombatSchedulerWorld(UserSkillRuntime& owner,UserSkillUseServices& skills,VerticalCombatWearNormalWorld& normal)
  :UserSkillRuntimeUseWorld(owner,skills),owner_(owner),normal_(normal){}
 void attack_mob(Actor&,MobRuntime&) final;
private:UserSkillRuntime& owner_;VerticalCombatWearNormalWorld& normal_;
};
}
