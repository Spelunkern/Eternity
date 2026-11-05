#pragma once
#include "eternity/game/vertical_combat_live_damage.hpp"
#include "eternity/game/mob_death.hpp"
#include "eternity/game/mob_applied_skills_catalog.hpp"
namespace eternity::game {
// CMob::Death 00491681 calls CMob::ClearApplySkill 004a0990 after
// resetting the mob damage ledger. The retained catalog, pool and logging
// ports must outlive the mob and all synchronous callbacks.
class VerticalCombatDeathAppliedSkillsWorld:public MobDeathWorld {
public:
 void clear_applied_skills(MobRuntime& mob) final {mob.applied_skills().clear();}
};
// Bind CMob::Damage's lethal continuation to the existing CMob::Death
// 004910f0 coordinator. Rules and the complete real reward, drop, user,
// transport, linked/obelisk and effect services remain retained, serialized
// and live across all synchronous death callbacks. No service has a default.
class VerticalCombatDeathServices:public VerticalCombatLiveDamageServices {
public:
 VerticalCombatDeathServices(const MobDeathExperience& rules,MobDeathWorld& world):rules_(rules),world_(world){}
 void death(MobRuntime& mob,std::int32_t type,std::uint32_t id) final {mob_death(mob,type,id,rules_,world_);}
private:
 const MobDeathExperience& rules_;MobDeathWorld& world_;
};
}
