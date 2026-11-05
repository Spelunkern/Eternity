#pragma once
#include "eternity/game/vertical_combat_eligibility.hpp"
#include "eternity/game/vertical_combat_skill_hit_prefix.hpp"
namespace eternity::game {
// Pure live borrows of the actor's original recursion byte (+57b4), jewel
// stItemInfo record, and level-100 stSkillInfo owner. Empty record/null skill
// means original lookup failure, never a synthetic success. Retain all owners
// through the entire operation, including nested hit and targeting callbacks.
class VerticalCombatItemOptionWorld {
public:
 virtual ~VerticalCombatItemOptionWorld()=default;
 virtual std::uint8_t& active(Actor&)=0;
 virtual std::span<const std::byte> jewel(std::uint8_t id)=0;
 virtual const UserSkillDefinition* mob_skill(std::uint8_t id)=0;
 virtual VerticalCombatSkillSelectWorld& select_world()=0;
 virtual VerticalCombatSkillHitPrefixWorld& hit_world()=0;
};
void vertical_combat_item_option(Actor&,MobRuntime&,VerticalCombatItemOptionWorld&);
}
