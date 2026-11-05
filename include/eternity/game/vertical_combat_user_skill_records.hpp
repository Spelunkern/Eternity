#pragma once
#include "eternity/game/vertical_combat_spend.hpp"
namespace eternity::game {
// Pure identity lookup for the exact retained stSkillInfo backing captured.
// Empty means no proven owner. Retain record and captured projection, and
// exclude reloads throughout combat callbacks. The original user table is
// distinct from the mob-skill table for levels 1..3.
class VerticalCombatUserSkillRecordSource {
public:
 virtual ~VerticalCombatUserSkillRecordSource()=default;
 virtual std::span<const std::byte> record(const UserSkillDefinition&) const=0;
};
VerticalCombatSkillRawDamage vertical_combat_user_skill_raw(const UserSkillDefinition&,const VerticalCombatUserSkillRecordSource&);
std::array<std::uint16_t,3> vertical_combat_user_skill_secondary(const UserSkillDefinition&,const VerticalCombatUserSkillRecordSource&);
VerticalCombatSkillSuccessInputs vertical_combat_user_skill_success_inputs(MobRuntime&,const UserSkillDefinition&,const VerticalCombatUserSkillRecordSource&,const MobRuntimeTemplates&,const MobTargetingBindingCatalog&);
class VerticalCombatUserSkillRecordsServices:public VerticalCombatSpendSkillServices {
public:
 VerticalCombatUserSkillRecordsServices(RandomSource&,VerticalCombatSkillRuntimeDamage&,MobTargetingZone&,const ItemDefinitions&,VerticalCombatView&,VerticalCombatHitServices&,VerticalCombatSkillSlotServices&,UserSkillSpendGroupWorld&,const VerticalCombatUserSkillRecordSource&,const MobRuntimeTemplates&,const MobTargetingBindingCatalog&);
 VerticalCombatSkillSuccessInputs success_inputs(MobRuntime&,const UserSkillDefinition&) final;
 VerticalCombatSkillRawDamage raw_skill_damage(const UserSkillDefinition&) final;
 std::array<std::uint16_t,3> secondary_words(const UserSkillDefinition&) final;
private:const VerticalCombatUserSkillRecordSource& skills_;const MobRuntimeTemplates& templates_;const MobTargetingBindingCatalog& mobs_;
};
}
