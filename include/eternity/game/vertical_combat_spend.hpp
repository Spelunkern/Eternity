#pragma once
#include "eternity/game/vertical_combat_skill_effect.hpp"
#include "eternity/game/actor_load.hpp"
#include "eternity/game/user_skill_runtime_spend_group.hpp"
namespace eternity::game {
// CUser::SendUseSPMP 00481ca0: actor/group/transport must remain retained and
// serialized through self and group sends, including synchronous callbacks.
void vertical_combat_spend(Actor&,std::uint16_t sp,std::uint16_t mp,UserSkillSpendGroupWorld&);
// 0048cfb0 recovery branches. next_tick is the actor's real +1484 scheduler owner.
// Keep actor, scene and scheduler alive and serialized through notifications.
// Recovery-only portion of live KC-status replacement at 0048ba63. Call at
// the original point after maximum resources have been updated and before
// later blessing fields, using the same retained actor and applied array.
void vertical_combat_replace_recovery_blessing(LoadedActor&,const BlessingDefinition&);
std::array<std::int32_t,3> vertical_combat_standing_recovery_amounts(const Actor&);
enum class VerticalCombatRecoveryResult { skipped, applied };
VerticalCombatRecoveryResult vertical_combat_recovery(LoadedActor&,std::uint32_t now,std::uint32_t& next_tick);
class VerticalCombatSpendSkillServices:public VerticalCombatAppliedSkillServices {
public:
 VerticalCombatSpendSkillServices(RandomSource&,VerticalCombatSkillRuntimeDamage&,MobTargetingZone&,const ItemDefinitions&,VerticalCombatView&,VerticalCombatHitServices&,VerticalCombatSkillSlotServices&,UserSkillSpendGroupWorld&);
 void spend(Actor&,std::uint16_t sp,std::uint16_t mp) final;
private:UserSkillSpendGroupWorld& group_;
};
}
