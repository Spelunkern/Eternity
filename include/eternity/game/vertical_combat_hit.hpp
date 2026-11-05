#pragma once
#include "eternity/game/vertical_combat_eligibility.hpp"
#include "eternity/game/mob_combat.hpp"
#include <optional>
namespace eternity::game {
// Real retained party membership and actor bonus, CMob::Damage and CMob::Death.
// party_id and aggro_bonus are pure reads of CUser +173c/+8 and +1230;
// damage must use the live mob, its own ledger and complete callbacks. Death
// must execute the actual 004910f0 owner, including its reward dependencies.
class VerticalCombatHitServices {
public:
 virtual ~VerticalCombatHitServices()=default;
 virtual std::optional<std::uint32_t> party_id(const Actor&)=0;
 virtual std::int32_t aggro_bonus(const Actor&)=0;
 virtual bool damage(Actor&,MobRuntime&,std::array<std::int32_t,3>&,const MobCombatCredit&)=0;
 virtual void death(MobRuntime&,std::int32_t type,std::uint32_t id)=0;
};
// AttackMob's normal, primary and secondary credit/damage/stop/death segment.
// party_damage stays live across callbacks; skill may be null for normal hits.
// Actor, party, mob, ledger, skill and damage storage survive all callbacks.
bool vertical_combat_hit(Actor&,MobRuntime&,const UserSkillDefinition*,std::array<std::int32_t,3>& party_damage,std::array<std::int32_t,3>& damage,VerticalCombatHitServices&);
class VerticalCombatHitNormalWorld:public VerticalCombatEquippedNormalWorld {
public:
 VerticalCombatHitNormalWorld(RandomSource&,ZoneUsers&,VerticalCombatRangedPassive&,std::span<const float,81>,MobTargetingZone&,const ItemDefinitions&,VerticalCombatView&,VerticalCombatHitServices&);
 bool apply_hit(Actor&,MobRuntime&,std::array<std::int32_t,3>&) final;
private:VerticalCombatHitServices& hits_;
};
class VerticalCombatHitSkillServices:public VerticalCombatViewSkillServices {
public:
 VerticalCombatHitSkillServices(RandomSource&,VerticalCombatSkillRuntimeDamage&,MobTargetingZone&,const ItemDefinitions&,VerticalCombatView&,VerticalCombatHitServices&);
 bool apply_primary(Actor&,MobRuntime&,const UserSkillDefinition&,std::array<std::int32_t,3>&) final;
 bool apply_secondary(Actor&,MobRuntime&,const UserSkillDefinition&,std::array<std::int32_t,3>&,std::array<std::int32_t,3>&) final;
private:VerticalCombatHitServices& hits_;
};
}
