#pragma once
#include "eternity/game/vertical_combat_eligibility.hpp"
#include "eternity/game/vertical_combat_scheduler.hpp"
#include "eternity/game/vertical_move_continuation.hpp"
#include <span>
namespace eternity::game {
// Real CMob::GetCurrentPos and mob definition size, and the shared GetTickCount.
// All objects must remain retained and serialized across these callbacks.
class VerticalCombatRangeServices {
public:
 virtual ~VerticalCombatRangeServices()=default;
 virtual UserPosition mob_current_position(MobRuntime&)=0;
 // Pure read of the retained mob definition byte +3f; no side effects.
 virtual std::uint8_t mob_size(const MobRuntime&)=0;
 virtual std::uint32_t tick_ms()=0;
};
// ps_game.exe CUser::EnableAttackRange 0044bf90 / 0044c030 and
// GetAttackEnableRange 0048a550. Factors are the live original five entries
// at 00542508. Requires finite arithmetic and the movement FP environment.
class VerticalCombatRange {
public:
 VerticalCombatRange(Actor&,ZoneUsers&,VerticalMotionState&,std::span<const float,5>,VerticalCombatRangeServices&);
 bool user(Actor&,Actor&,std::int32_t extra);
 bool mob(Actor&,MobRuntime&,std::int32_t extra);
private:
 UserPosition position(Actor&) const;
 float allowance();
 Actor& actor_;ZoneUsers& zone_;VerticalMotionState& movement_;
 std::span<const float,5> factors_;VerticalCombatRangeServices& services_;
};
// Skill eligibility retains this same range owner; cast transports remain required.
class VerticalCombatRangeEligibilityServices:public VerticalCombatEligibilityServices {
public:
 VerticalCombatRangeEligibilityServices(UserSkillRuntime&,VerticalCombatViewSkillServices&,VerticalCombatRange&);
 bool in_user_range(Actor&,Actor&,std::uint8_t) final;
 bool in_mob_range(Actor&,MobRuntime&,std::uint8_t) final;
private:VerticalCombatRange& range_;
};
// Normal scheduler checks use zero additional range; target eligibility and
// all other scheduler services remain required actual providers.
class VerticalCombatRangeSchedulerWorld:public VerticalCombatSchedulerWorld {
public:
 VerticalCombatRangeSchedulerWorld(UserSkillRuntime&,UserSkillUseServices&,VerticalCombatWearNormalWorld&,VerticalCombatRange&);
 bool in_range(Actor&,MobRuntime&) final;
 bool in_user_range(Actor&,Actor&) final;
private:VerticalCombatRange& range_;
};
}
