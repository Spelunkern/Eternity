#pragma once
#include "eternity/game/vertical_combat_range.hpp"
#include "eternity/game/mob_targeting_bindings.hpp"
#include "eternity/game/zone_mobs.hpp"
namespace eternity::game {
// CUser::EnableAttackMob 0044cd80; actor and mob grids are the same retained
// zone, and the mob's runtime template and catalog are the actual paired data.
// Actor, target, zone and definition owners remain pinned and serialized for
// the entire scheduler poll, including all synchronous callbacks.
MobRuntime* vertical_combat_enable_mob(Actor&,std::uint32_t,ZoneUsers&,ZoneMobs&,const MobRuntimeTemplates&,const MobTargetingBindingCatalog&);
// CMob::IsBothmob 0044c610, a pure read of live mob/respawn state.
bool vertical_combat_is_both_mob(const MobRuntime&) noexcept;
class VerticalCombatTargetSchedulerWorld:public VerticalCombatRangeSchedulerWorld {
public:
 VerticalCombatTargetSchedulerWorld(UserSkillRuntime&,UserSkillUseServices&,VerticalCombatWearNormalWorld&,VerticalCombatRange&,ZoneUsers&,ZoneMobs&,const MobRuntimeTemplates&,const MobTargetingBindingCatalog&);
 MobRuntime* enable_mob(Actor&,std::uint32_t) final;
 bool is_both_mob(MobRuntime&) final;
 Actor* find_attack_user(Actor&,std::uint32_t) final;
private:
 void require_actor(const Actor&)const;
 UserSkillRuntime& owner_;ZoneUsers& users_;ZoneMobs& mobs_;
 const MobRuntimeTemplates& templates_;const MobTargetingBindingCatalog& catalog_;
};
}
