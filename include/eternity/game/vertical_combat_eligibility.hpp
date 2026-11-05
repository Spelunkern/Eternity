#pragma once
#include "eternity/game/user_skill_runtime_equipment.hpp"
#include "eternity/game/vertical_combat_view.hpp"
namespace eternity::game {
// Pure borrow of actual retained bag-0 weapon/shield. A value-only inventory
// mirror is not an absent item; it is an error until its pooled owner is bound.
// The actor, item pool and definition outlive the synchronous eligibility call.
const GroundItem* vertical_combat_equipped(Actor&,std::uint16_t slot);
// The normal AttackMob caller reads its live equipped weapon after sleep
// removal. A missing or unbound retained weapon fails instead of inventing one.
class VerticalCombatEquippedNormalWorld:public VerticalCombatViewNormalWorld {
public:
 using VerticalCombatViewNormalWorld::VerticalCombatViewNormalWorld;
 const ItemDefinition& weapon(Actor&) final;
};
// Complete recovered cost, equipment and readiness coordination for the same
// actor/runtime. Range and cast transports remain required actual providers.
// The action owner, runtime and shared clock/RNG remain retained and serialized.
class VerticalCombatEligibilityServices:public UserSkillRuntimeEligibilityServices {
public:
 VerticalCombatEligibilityServices(UserSkillRuntime& owner,VerticalCombatViewSkillServices& actions);
 const GroundItem* equipped(Actor&,std::uint16_t slot) final;
 void user_effect(UserSkillAction,Actor&,Actor*,const UserSkillDefinition&) final;
 void mob_effect(UserSkillAction,Actor&,MobRuntime*,const UserSkillDefinition&) final;
 void spend(Actor&,std::uint16_t sp,std::uint16_t mp) final;
 std::uint32_t tick_ms() final;
private:
 void require_actor(const Actor&) const;
 UserSkillRuntime& owner_;VerticalCombatViewSkillServices& actions_;
};
}
