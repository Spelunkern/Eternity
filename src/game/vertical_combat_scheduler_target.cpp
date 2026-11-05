#include "eternity/game/vertical_combat_scheduler_target.hpp"
#include <stdexcept>
namespace eternity::game {
namespace {
bool has_zone(const Actor& actor,ZoneUsers& users){
 if(!actor.status().in_zone)return false;
 if(users.find_actor(actor.id())!=&actor)throw std::logic_error("Combat actor lacks retained zone membership");
 return true;
}
}
MobRuntime* vertical_combat_enable_mob(Actor& actor,std::uint32_t id,ZoneUsers& users,ZoneMobs& mobs,const MobRuntimeTemplates& templates,const MobTargetingBindingCatalog& catalog){
 if(!has_zone(actor,users))return nullptr;
 auto* mob=mobs.find(id);if(!mob)return nullptr;
 if(mob->zone!=&mobs)throw std::logic_error("Combat mob lacks its retained zone");
 if(mob_status(*mob)==2)return nullptr;
 if(!mob->definition || templates.find(mob->definition->id)!=mob->definition)throw std::logic_error("Combat mob lacks its retained template");
 const auto* definition=catalog.mob(mob->definition->id);
 if(!definition||definition->id!=mob->definition->id)throw std::logic_error("Combat mob lacks its matching catalog record");
 std::uint32_t country=0;for(unsigned i=0;i<4;++i)country|=std::uint32_t(std::to_integer<unsigned char>(definition->record[0x78+i]))<<(8*i);
 return country==actor.appearance().identity.country?nullptr:mob;
}
bool vertical_combat_is_both_mob(const MobRuntime& mob)noexcept{return mob.lua_ai!=0 || (mob.respawn && mob.respawn->both);}
VerticalCombatTargetSchedulerWorld::VerticalCombatTargetSchedulerWorld(UserSkillRuntime& o,UserSkillUseServices& s,VerticalCombatWearNormalWorld& n,VerticalCombatRange& r,ZoneUsers& u,ZoneMobs& m,const MobRuntimeTemplates& t,const MobTargetingBindingCatalog& c):VerticalCombatRangeSchedulerWorld(o,s,n,r),owner_(o),users_(u),mobs_(m),templates_(t),catalog_(c){}
void VerticalCombatTargetSchedulerWorld::require_actor(const Actor& actor)const{if(&actor!=&owner_.actor())throw std::invalid_argument("Combat scheduler requires its retained actor");}
MobRuntime* VerticalCombatTargetSchedulerWorld::enable_mob(Actor& a,std::uint32_t id){require_actor(a);return vertical_combat_enable_mob(a,id,users_,mobs_,templates_,catalog_);}
bool VerticalCombatTargetSchedulerWorld::is_both_mob(MobRuntime& m){return vertical_combat_is_both_mob(m);}
Actor* VerticalCombatTargetSchedulerWorld::find_attack_user(Actor& a,std::uint32_t id){require_actor(a);return has_zone(a,users_)?users_.find_actor(id):nullptr;}
}
