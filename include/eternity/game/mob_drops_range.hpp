#pragma once
#include "eternity/game/mob_drops.hpp"

namespace eternity::game {
// Zone identities must project original CZone pointer identity consistently.
// Both users are retained non-null projections; original helpers dereference
// them and do not provide a missing-user fallback.
// ps_game.exe CMob::EnableApplyZoneItem 004a6560. Ignores death and distance.
bool mob_drop_zone_enabled(std::uint32_t mob_zone,const MobLootUser& credited,const MobLootUser& candidate);
// CMob::EnableApplyRangeItem 004a64f0, SVector::Distance2 0041b1f0.
// Only candidate dead==1 is rejected. X/Z differences round to float before
// double squares/sqrt; result and integer radius round to float. The original
// x87 condition accepts unordered (NaN) comparisons as well as <=.
bool mob_drop_range_enabled(std::uint32_t mob_zone,const MobLootUser& credited,const MobLootUser& candidate,std::int32_t radius);
// Inline party-money gate at 004a6311..004a63aa: candidate eligibility only;
// credited state/mob-zone gates are deliberately absent. Radius is fixed at 100.
bool mob_drop_money_range_enabled(const MobLootUser& credited,const MobLootUser& candidate);
}
