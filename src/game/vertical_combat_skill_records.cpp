#include "eternity/game/vertical_combat_skill_records.hpp"
#include <stdexcept>
namespace eternity::game {
namespace {
const MobTargetingBindingSkill& record(const UserSkillDefinition& skill,const MobTargetingBindingCatalog& catalog){
 const auto* slot=catalog.from_definition(&skill.effect);
 if(!slot)throw std::invalid_argument("Skill combat requires the exact retained definition record");
 return *slot;
}
std::uint8_t byte(const std::byte* data,std::size_t offset){return std::to_integer<std::uint8_t>(data[offset]);}
std::uint16_t word(const std::byte* data,std::size_t offset){return static_cast<std::uint16_t>(byte(data,offset)|(static_cast<unsigned>(byte(data,offset+1))<<8));}
}
VerticalCombatSkillRawDamage vertical_combat_skill_raw(const UserSkillDefinition& skill,const MobTargetingBindingCatalog& catalog){
 const auto* data=record(skill,catalog).record.data();return {byte(data,0x66),{word(data,0x68),word(data,0x6a),word(data,0x6c)}};
}
std::array<std::uint16_t,3> vertical_combat_skill_secondary(const UserSkillDefinition& skill,const MobTargetingBindingCatalog& catalog){
 const auto* data=record(skill,catalog).record.data();return {word(data,0x76),word(data,0x78),word(data,0x7a)};
}
VerticalCombatSkillSuccessInputs vertical_combat_skill_success_inputs(MobRuntime& mob,const UserSkillDefinition& skill,const MobRuntimeTemplates& templates,const MobTargetingBindingCatalog& catalog){
 const auto& s=record(skill,catalog);
 if(!mob.definition || templates.find(mob.definition->id)!=mob.definition)throw std::invalid_argument("Skill success requires the retained mob template");
 const auto* info=catalog.mob(mob.definition->id);
 if(!info || info->id!=mob.definition->id)throw std::logic_error("Skill success mob definition is unavailable or mismatched");
 const auto* data=reinterpret_cast<const std::uint8_t*>(info->record.data());
 return {skill,{data+0x46,info->record.size()-0x46},{data+0x55,6},byte(s.record.data(),0x54),byte(s.record.data(),0x55)};
}
VerticalCombatSkillRecordsServices::VerticalCombatSkillRecordsServices(RandomSource& r,VerticalCombatSkillRuntimeDamage& d,MobTargetingZone& z,const ItemDefinitions& i,VerticalCombatView& v,VerticalCombatHitServices& h,VerticalCombatSkillSlotServices& s,UserSkillSpendGroupWorld& g,const MobRuntimeTemplates& t,const MobTargetingBindingCatalog& c):VerticalCombatSpendSkillServices(r,d,z,i,v,h,s,g),templates_(t),catalog_(c){}
VerticalCombatSkillSuccessInputs VerticalCombatSkillRecordsServices::success_inputs(MobRuntime& m,const UserSkillDefinition& s){return vertical_combat_skill_success_inputs(m,s,templates_,catalog_);}
VerticalCombatSkillRawDamage VerticalCombatSkillRecordsServices::raw_skill_damage(const UserSkillDefinition& s){return vertical_combat_skill_raw(s,catalog_);}
std::array<std::uint16_t,3> VerticalCombatSkillRecordsServices::secondary_words(const UserSkillDefinition& s){return vertical_combat_skill_secondary(s,catalog_);}
}
