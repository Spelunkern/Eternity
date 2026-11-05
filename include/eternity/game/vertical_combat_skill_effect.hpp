#pragma once
#include "eternity/game/vertical_combat_hit.hpp"
#include "eternity/game/mob_applied_skills_catalog.hpp"
namespace eternity::game {
// Exact authoritative binding of the captured user stSkillInfo to the same
// slot and retained definition owned by this mob's applied-skills catalog.
// Resolve must be a pure lookup; ID/level coincidence is insufficient. Return
// no binding when the actual owner cannot prove identity. The returned catalog
// must be the very one retained by mob.applied_skills(); that owner does not
// expose its catalog identity, so root must enforce this assembly invariant.
// The provider, actor,
// mob, definition, catalog and pool survive synchronous callbacks and reloads
// are excluded during the operation.
struct VerticalCombatSkillSlot {std::uint32_t slot;const MobTargetingBindingCatalog* catalog;};
class VerticalCombatSkillSlotServices {
public:
 virtual ~VerticalCombatSkillSlotServices()=default;
 virtual std::optional<VerticalCombatSkillSlot> resolve(const MobRuntime&,const UserSkillDefinition&)=0;
};
// AttackMob 00450574 -> CMob::AddApplySkill 004a0aa0, attacker kind 1.
// Missing/foreign owners are errors; a catalog-admitted skill may still be
// rejected by the original effect filters, as in the existing owner.
void vertical_combat_add_mob_skill(MobRuntime&,Actor&,const UserSkillDefinition&,VerticalCombatSkillSlotServices&);
class VerticalCombatAppliedSkillServices:public VerticalCombatHitSkillServices {
public:
 VerticalCombatAppliedSkillServices(RandomSource&,VerticalCombatSkillRuntimeDamage&,MobTargetingZone&,const ItemDefinitions&,VerticalCombatView&,VerticalCombatHitServices&,VerticalCombatSkillSlotServices&);
 void add_mob_skill(MobRuntime&,Actor&,const UserSkillDefinition&) final;
private:VerticalCombatSkillSlotServices& slots_;
};
}
