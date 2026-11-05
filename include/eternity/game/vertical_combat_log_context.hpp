#pragma once
#include "eternity/game/vertical_combat_log.hpp"
#include "eternity/game/zones.hpp"
#include <stdexcept>
namespace eternity::game {
// CMob::SendLogBothMob 004a1810 reads the actual zone map ID and the mob's
// original definition name at +2. Root retains the exact zone, template and
// catalog used for this mob, loads them from the same original mob records,
// and serializes catalog reloads and all callbacks across synchronous sends.
class VerticalCombatRetainedLogContext final:public VerticalCombatMobLogContextServices {
public:
 VerticalCombatRetainedLogContext(RuntimeZone& zone,const MobRuntimeTemplates& templates,const MobTargetingBindingCatalog& catalog)
  :zone_(zone),templates_(templates),catalog_(catalog){}
 MobLogContext context(const MobRuntime& mob) final {
  if(!zone_.mobs || mob.zone!=zone_.mobs.get() || !zone_.map || zone_.map->settings.id<0 || zone_.map->settings.id>65535)
   throw std::invalid_argument("Combat log requires the mob's retained zone and map");
  if(!mob.definition || templates_.find(mob.definition->id)!=mob.definition)
   throw std::invalid_argument("Combat log requires the retained mob template");
  const auto* definition=catalog_.mob(mob.definition->id);
  if(!definition || definition->id!=mob.definition->id)
   throw std::logic_error("Combat log mob definition is unavailable or mismatched");
  const std::string_view name(reinterpret_cast<const char*>(definition->record.data()+2),32);
  return {static_cast<std::uint16_t>(zone_.map->settings.id),name};
 }
private:RuntimeZone& zone_;const MobRuntimeTemplates& templates_;const MobTargetingBindingCatalog& catalog_;
};
}
