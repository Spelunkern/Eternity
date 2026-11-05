#include "eternity/game/vertical_combat_mob_stop_retained.hpp"
#include "eternity/game/mob_targeting_find.hpp"
#include <limits>
#include <stdexcept>
namespace eternity::game {
VerticalCombatMobStopRetainedWorld::VerticalCombatMobStopRetainedWorld(MobRuntime& mob,ZoneMobs& mobs,ZoneUsers& users,const MobRuntimeTemplates& templates,const MobTargetingBindingCatalog& catalog,MobTargetingZone& targeting,MobAiConfiguredFsm& fsm,const MobAiVariableCatalog& variables,MobAiConfiguredServices& ai,VerticalCombatMobTargetLease& leases):mob_(mob),mobs_(mobs),users_(users),templates_(templates),catalog_(catalog),targeting_(targeting),fsm_(fsm),variables_(variables),ai_(ai),leases_(leases){}
void VerticalCombatMobStopRetainedWorld::require_mob(const MobRuntime& mob)const{
 if(&mob!=&mob_ || mob.zone!=&mobs_ || !mob.cell || mobs_.find(mob.id)!=&mob || !mob.definition || templates_.find(mob.definition->id)!=mob.definition)throw std::invalid_argument("Mob stop requires its retained zone and template");
}
bool VerticalCombatMobStopRetainedWorld::find_target(MobRuntime& mob){
 require_mob(mob);const auto* definition=catalog_.mob(mob.definition->id);
 if(!definition || definition->id!=mob.definition->id)throw std::logic_error("Mob stop requires its retained definition record");
 const auto count=users_.size();if(count>static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()))throw std::overflow_error("Mob target count overflow");
 MobTargetingFindSource source{static_cast<std::int32_t>(count),mobs_.cell_count(),mobs_.cell_count(),mob.cell->x,mob.cell->z,mob.position.x,mob.position.z,definition->ai,std::to_integer<std::uint8_t>(definition->record[0x69])};
 struct Find final:MobTargetingFindServices {
  MobRuntime& mob;ZoneUsers& users;MobTargetingZone& targeting;VerticalCombatMobTargetLease& leases;MobTargetingFindSource& source;const MobRuntimeTemplates& templates;const MobTargetingBindingCatalog& catalog;Actor* current=nullptr;
  Find(MobRuntime& m,ZoneUsers& u,MobTargetingZone& t,VerticalCombatMobTargetLease& l,MobTargetingFindSource& s,const MobRuntimeTemplates& ts,const MobTargetingBindingCatalog& c):mob(m),users(u),targeting(t),leases(l),source(s),templates(ts),catalog(c){}
  void visit_cell_users(std::int32_t x,std::int32_t z,const std::function<bool(const MobTargetingFindUser&)>& visitor)override{
   users.visit_cell(x,z,[&](Actor& actor,UserPosition position){
    auto pin=leases.retain(actor);if(!pin)throw std::logic_error("Mob target actor lease is required");
    if(!mob.definition || templates.find(mob.definition->id)!=mob.definition)throw std::logic_error("Mob target lost its retained definition");
    const auto* live=catalog.mob(mob.definition->id);if(!live || live->id!=mob.definition->id)throw std::logic_error("Mob target lost its retained record");
    current=&actor;source.x=mob.position.x;source.z=mob.position.z;source.radius=std::to_integer<std::uint8_t>(live->record[0x69]);
    const auto& a=actor.appearance();const bool more=visitor({reinterpret_cast<std::uintptr_t>(&actor),actor.attack_state().mode,actor.status().shape,static_cast<std::uint8_t>(actor.status().no_mob_targeting),a.attackable,a.visible,position.x,position.z});
    current=nullptr;return !more;
   });
  }
  void set_target_with_help(std::uintptr_t token)override{
   if(!current || token!=reinterpret_cast<std::uintptr_t>(current))throw std::logic_error("Mob target lease identity mismatch");
   MobTargetingActorUser user(*current);targeting.set_target(mob,user,true);
  }
 } services(mob,users_,targeting_,leases_,source,templates_,catalog_);
 return mob_find_target(source,services)!=0;
}
void VerticalCombatMobStopRetainedWorld::reset_fsm(MobRuntime& mob){require_mob(mob);reset_mob_ai_configured(mob,fsm_,variables_,ai_);}
}
