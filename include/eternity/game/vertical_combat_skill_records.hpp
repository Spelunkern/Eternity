#pragma once
#include "eternity/game/vertical_combat_spend.hpp"
namespace eternity::game {
// The exact catalog owns captured.effect; mob templates and catalog are the
// same retained projections used at spawn. Serialize reloads across all reads.
VerticalCombatSkillRawDamage vertical_combat_skill_raw(const UserSkillDefinition&,const MobTargetingBindingCatalog&);
std::array<std::uint16_t,3> vertical_combat_skill_secondary(const UserSkillDefinition&,const MobTargetingBindingCatalog&);
VerticalCombatSkillSuccessInputs vertical_combat_skill_success_inputs(MobRuntime&,const UserSkillDefinition&,const MobRuntimeTemplates&,const MobTargetingBindingCatalog&);
class VerticalCombatSkillRecordsServices:public VerticalCombatSpendSkillServices {
public:
 VerticalCombatSkillRecordsServices(RandomSource&,VerticalCombatSkillRuntimeDamage&,MobTargetingZone&,const ItemDefinitions&,VerticalCombatView&,VerticalCombatHitServices&,VerticalCombatSkillSlotServices&,UserSkillSpendGroupWorld&,const MobRuntimeTemplates&,const MobTargetingBindingCatalog&);
 VerticalCombatSkillSuccessInputs success_inputs(MobRuntime&,const UserSkillDefinition&) final;
 VerticalCombatSkillRawDamage raw_skill_damage(const UserSkillDefinition&) final;
 std::array<std::uint16_t,3> secondary_words(const UserSkillDefinition&) final;
private:const MobRuntimeTemplates& templates_;const MobTargetingBindingCatalog& catalog_;
};
}
