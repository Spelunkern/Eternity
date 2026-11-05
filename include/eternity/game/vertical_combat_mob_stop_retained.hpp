#pragma once
#include "eternity/game/vertical_combat_mob_stop.hpp"
#include "eternity/game/mob_ai_states_reset.hpp"
#include "eternity/game/mob_targeting_actor.hpp"
namespace eternity::game {
class VerticalCombatMobTargetLease {
public:
 virtual ~VerticalCombatMobTargetLease()=default;
 // Pure actor lease, including scene and membership, through SetTarget callbacks.
 virtual std::shared_ptr<void> retain(Actor&)=0;
};
// Bound to one actual mob, paired grids, spawn template/catalog and FSM. The
// caller serializes all owners and retains the mob through the whole stop.
class VerticalCombatMobStopRetainedWorld final:public VerticalCombatMobStopWorld {
public:
 VerticalCombatMobStopRetainedWorld(MobRuntime&,ZoneMobs&,ZoneUsers&,const MobRuntimeTemplates&,const MobTargetingBindingCatalog&,MobTargetingZone&,MobAiConfiguredFsm&,const MobAiVariableCatalog&,MobAiConfiguredServices&,VerticalCombatMobTargetLease&);
 bool find_target(MobRuntime&) override;
 void reset_fsm(MobRuntime&) override;
private:
 void require_mob(const MobRuntime&) const;
 MobRuntime& mob_;ZoneMobs& mobs_;ZoneUsers& users_;const MobRuntimeTemplates& templates_;const MobTargetingBindingCatalog& catalog_;MobTargetingZone& targeting_;
 MobAiConfiguredFsm& fsm_;const MobAiVariableCatalog& variables_;MobAiConfiguredServices& ai_;VerticalCombatMobTargetLease& leases_;
};
}
