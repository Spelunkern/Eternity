#include "eternity/game/vertical_combat_user_skill_records.hpp"
#include <stdexcept>
namespace eternity::game {
namespace {
std::span<const std::byte> record(const UserSkillDefinition& skill,const VerticalCombatUserSkillRecordSource& source){
 auto bytes=source.record(skill);if(bytes.size()!=160)throw std::logic_error("Combat requires the exact retained user skill record");return bytes;
}
std::uint8_t byte(std::span<const std::byte> data,std::size_t offset){return std::to_integer<std::uint8_t>(data[offset]);}
std::uint16_t word(std::span<const std::byte> data,std::size_t offset){return static_cast<std::uint16_t>(byte(data,offset)|(static_cast<unsigned>(byte(data,offset+1))<<8));}
}
VerticalCombatSkillRawDamage vertical_combat_user_skill_raw(const UserSkillDefinition& skill,const VerticalCombatUserSkillRecordSource& source){
 const auto b=record(skill,source);return {byte(b,0x66),{word(b,0x68),word(b,0x6a),word(b,0x6c)}};
}
std::array<std::uint16_t,3> vertical_combat_user_skill_secondary(const UserSkillDefinition& skill,const VerticalCombatUserSkillRecordSource& source){
 const auto b=record(skill,source);return {word(b,0x76),word(b,0x78),word(b,0x7a)};
}
VerticalCombatSkillSuccessInputs vertical_combat_user_skill_success_inputs(MobRuntime& mob,const UserSkillDefinition& skill,const VerticalCombatUserSkillRecordSource& source,const MobRuntimeTemplates& templates,const MobTargetingBindingCatalog& mobs){
 const auto b=record(skill,source);
 if(!mob.definition || templates.find(mob.definition->id)!=mob.definition)throw std::invalid_argument("Skill success requires the retained mob template");
 const auto* info=mobs.mob(mob.definition->id);if(!info || info->id!=mob.definition->id)throw std::logic_error("Skill success requires the retained mob record");
 const auto* data=reinterpret_cast<const std::uint8_t*>(info->record.data());
 return {skill,{data+0x46,info->record.size()-0x46},{data+0x55,6},byte(b,0x54),byte(b,0x55)};
}
VerticalCombatUserSkillRecordsServices::VerticalCombatUserSkillRecordsServices(RandomSource& r,VerticalCombatSkillRuntimeDamage& d,MobTargetingZone& z,const ItemDefinitions& i,VerticalCombatView& v,VerticalCombatHitServices& h,VerticalCombatSkillSlotServices& s,UserSkillSpendGroupWorld& g,const VerticalCombatUserSkillRecordSource& u,const MobRuntimeTemplates& t,const MobTargetingBindingCatalog& m):VerticalCombatSpendSkillServices(r,d,z,i,v,h,s,g),skills_(u),templates_(t),mobs_(m){}
VerticalCombatSkillSuccessInputs VerticalCombatUserSkillRecordsServices::success_inputs(MobRuntime& m,const UserSkillDefinition& s){return vertical_combat_user_skill_success_inputs(m,s,skills_,templates_,mobs_);}
VerticalCombatSkillRawDamage VerticalCombatUserSkillRecordsServices::raw_skill_damage(const UserSkillDefinition& s){return vertical_combat_user_skill_raw(s,skills_);}
std::array<std::uint16_t,3> VerticalCombatUserSkillRecordsServices::secondary_words(const UserSkillDefinition& s){return vertical_combat_user_skill_secondary(s,skills_);}
}
