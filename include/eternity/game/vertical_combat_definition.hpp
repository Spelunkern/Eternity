#pragma once
#include "eternity/game/vertical_combat_retarget.hpp"
#include "eternity/game/mob_targeting_bindings.hpp"
#include <stdexcept>
namespace eternity::game {
// CMob::Init 00490040 obtains the mob definition via GetMobInfo 004026d0.
// Borrow the same retained runtime-template and targeting-catalog owners used
// by mob spawning/targeting. Root must load them from the same original mob
// rows, coordinate reloads and preserve their lifetimes across all callbacks.
class VerticalCombatDefinitionServices:public VerticalCombatAggroExServices {
public:
 VerticalCombatDefinitionServices(Actor& actor,PartyMembershipUser& member,const MobDeathExperience& rules,MobDeathWorld& world,MobAiStateServices& fsm,VerticalCombatClock& clock,ZoneMobs& mobs,ZoneUsers& users,const MobRuntimeTemplates& templates,const MobTargetingBindingCatalog& catalog)
  :VerticalCombatAggroExServices(actor,member,rules,world,fsm,clock,mobs,users),templates_(templates),catalog_(catalog){}
 std::span<const std::byte> mob_definition_record(const MobRuntime& mob) final {
  if(!mob.definition || templates_.find(mob.definition->id)!=mob.definition)throw std::invalid_argument("Combat requires the retained mob template");
  const auto* definition=catalog_.mob(mob.definition->id);
  if(!definition || definition->id!=mob.definition->id)throw std::logic_error("Combat mob definition record is unavailable or mismatched");
  return definition->record;
 }
private:
 const MobRuntimeTemplates& templates_;const MobTargetingBindingCatalog& catalog_;
};
}
