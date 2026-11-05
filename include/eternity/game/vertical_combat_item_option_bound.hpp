#pragma once
#include "eternity/game/vertical_combat_item_option.hpp"
#include "eternity/game/vertical_combat_hit.hpp"
#include "eternity/game/vertical_combat_mob_stop.hpp"
namespace eternity::game {
// The guard is the actual actor's retained CUser+57b4 byte. Jewel and skill
// lookups remain mandatory live catalog providers. Both action paths use the
// same selected-skill owner and its retained view/hit/target services.
class VerticalCombatItemOptionBoundWorld:public VerticalCombatItemOptionWorld {
public:
 VerticalCombatItemOptionBoundWorld(Actor&,std::uint8_t&,VerticalCombatSkillHitSelectServices&);
 std::uint8_t& active(Actor&) final;
 VerticalCombatSkillSelectWorld& select_world() final;
 VerticalCombatSkillHitPrefixWorld& hit_world() final;
private:Actor& actor_;std::uint8_t& guard_;VerticalCombatSkillHitSelectServices& skills_;
};
// Normal mob hit uses the same item-option coordinator as skill hits.
class VerticalCombatItemOptionNormalWorld:public VerticalCombatHitNormalWorld {
public:
 VerticalCombatItemOptionNormalWorld(RandomSource&,ZoneUsers&,VerticalCombatRangedPassive&,std::span<const float,81>,MobTargetingZone&,const ItemDefinitions&,VerticalCombatView&,VerticalCombatHitServices&,VerticalCombatItemOptionWorld&);
 void attack_item_option(Actor&,MobRuntime&) final;
private:VerticalCombatItemOptionWorld& options_;
};
// Skill-hit dispatch can return the same bound world from its live root. This
// accessor is required to resolve the self-reference after construction.
class VerticalCombatItemOptionSkillServices:public VerticalCombatMobStopServices {
public:
 using VerticalCombatMobStopServices::VerticalCombatMobStopServices;
 void attack_item_option(Actor&,MobRuntime&) final;
 virtual VerticalCombatItemOptionWorld& item_option_world()=0;
};
}
