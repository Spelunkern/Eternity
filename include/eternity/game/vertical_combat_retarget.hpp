#pragma once
#include "eternity/game/vertical_combat_mob_state.hpp"
#include "eternity/game/zone_mobs.hpp"
namespace eternity::game {
// CMob::ReTargetByAggro 00490ab0 and ReTargetByAggroEx 004b7490.
// Bind actual paired zone grids and shared clock. For stacked AI the exact
// GetMaxAggroEx 004b74f0 owner is required and may reenter/mutate the mob.
// Retain the mob, ledger, definition and any user across synchronous calls;
// zone access and callbacks execute on the same serialized world executor.
class VerticalCombatRetargetServices:public VerticalCombatMobStateServices {
public:
 VerticalCombatRetargetServices(Actor&,PartyMembershipUser&,const MobDeathExperience&,MobDeathWorld&,MobAiStateServices&,VerticalCombatClock&,ZoneMobs&,ZoneUsers&);
 void retarget(MobRuntime&,bool stacked) final;
 virtual std::uint32_t maximum_aggro_ex(MobRuntime&)=0;
protected:ZoneMobs& mobs_;ZoneUsers& users_;
};
// CMob::GetMaxAggroEx 004b74f0 scans the retained ledger in insertion order.
// The mob, ledger records, zone users, definition and all selected actors stay
// retained and serialized; lookup and definition reads must not reenter and
// mutate the ledger. The paired grids must belong to the same actual zone.
class VerticalCombatAggroExServices:public VerticalCombatRetargetServices {
public:
 using VerticalCombatRetargetServices::VerticalCombatRetargetServices;
 std::uint32_t maximum_aggro_ex(MobRuntime&) final;
};
}
