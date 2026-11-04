#include "eternity/game/mob_applied_skills_catalog.hpp"
#include <exception>
namespace eternity::game {
MobAppliedSkillsCatalog::MobAppliedSkillsCatalog(const MobTargetingBindingCatalog& c,MobAppliedSkillPool& p,
    MobRuntime& m,std::atomic<std::uint32_t>& ids,MobAppliedSkillsCatalogPorts& ports)
    :catalog_(c),mob_(m),counter_(ids),ports_(ports),owner_(p,*this){}
const MobTargetingBindingSkill& MobAppliedSkillsCatalog::slot(const SkillDefinition& d)const noexcept {
    const auto* s=catalog_.from_definition(&d);if(!s)std::terminate();return *s;
}
std::string_view MobAppliedSkillsCatalog::name(const SkillDefinition& d)const noexcept{return slot(d).name();}
std::uint8_t MobAppliedSkillsCatalog::base_attribute()const noexcept{if(!mob_.definition)std::terminate();return mob_.definition->attribute;}
void MobAppliedSkillsCatalog::synchronize_names(){
    for(auto* s:owner_.records()){if(!s->definition)std::terminate();s->name=name(*s->definition);}
}
bool MobAppliedSkillsCatalog::apply(std::uint32_t id,std::uint32_t type,std::uint32_t attacker){
    const auto* d=catalog_.skill_definition(id);if(!d)return false;
    synchronize_names();
    // Admission copies this string into CSkill. The definition itself is always
    // the catalog's embedded object. Refresh intentionally retains node payload.
    const std::string text(name(*d));
    owner_.apply(mob_,{*d,text,type,attacker},base_attribute(),counter_,*this,*this);return true;
}
void MobAppliedSkillsCatalog::keep(std::uint32_t now){synchronize_names();owner_.keep(now,mob_,*this,*this);}
void MobAppliedSkillsCatalog::expire(std::uint32_t now){synchronize_names();owner_.expire(now,{mob_.lua_ai!=0,mob_.respawn!=nullptr,mob_.respawn && mob_.respawn->both},*this);}
void MobAppliedSkillsCatalog::clear(){owner_.clear({mob_.lua_ai!=0,mob_.respawn!=nullptr,mob_.respawn && mob_.respawn->both});}
std::uint32_t MobAppliedSkillsCatalog::tick()noexcept{return ports_.tick();}
void MobAppliedSkillsCatalog::removed(const MobAppliedSkill& s,std::uint8_t a)noexcept{if(!s.definition)std::terminate();ports_.log(s,name(*s.definition),a);}
void MobAppliedSkillsCatalog::added(const MobAppliedSkill& s,std::uint8_t a)noexcept{removed(s,a);}
void MobAppliedSkillsCatalog::rebuild_attack(MobRuntime& m)noexcept{ports_.rebuild_attack(m);}
void MobAppliedSkillsCatalog::send_speed(MobRuntime& m)noexcept{ports_.send_speed(m);}
void MobAppliedSkillsCatalog::finish_damage_skill(MobRuntime& m,MobAppliedSkill& s,const SkillDefinition& d)noexcept{
    s.name=name(d);finish_mob_applied_skill_damage(m,s,d,*this);
}
MobAppliedSkillDamage MobAppliedSkillsCatalog::time_damage(MobRuntime& m,const SkillDefinition& d,std::uint32_t elapsed)noexcept{
    const auto& s=slot(d);if(!m.definition)std::terminate();
    return calculate_mob_applied_skill_time_damage({s.time_damage_type,s.time_damage},project_mob_applied_skill_time_damage(m,*m.definition),elapsed);
}
bool MobAppliedSkillsCatalog::damage_periodic(MobRuntime& m,std::uint32_t t,std::uint32_t id,MobAppliedSkillDamage& d)noexcept{return ports_.damage_periodic(m,t,id,d);}
bool MobAppliedSkillsCatalog::damage_hp(MobRuntime& m,std::uint32_t t,std::uint32_t id,std::uint32_t hp)noexcept{return ports_.damage_hp(m,t,id,hp);}
void MobAppliedSkillsCatalog::death(MobRuntime& m,std::uint32_t t,std::uint32_t id)noexcept{ports_.death(m,t,id);}
void MobAppliedSkillsCatalog::send_result(MobRuntime& m,const MobAppliedSkillDamagePacket& p,float r,std::uint8_t priority)noexcept{ports_.send_result(m,p,r,priority);}
void MobAppliedSkillsCatalog::remove_effects(MobAppliedSkill& s)noexcept{if(!s.definition)std::terminate();remove_mob_applied_skill_effects(mob_,s,*s.definition,base_attribute(),*this);}
}
