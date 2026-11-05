#include "eternity/game/vertical_combat_mob_state.hpp"
#include <stdexcept>
namespace eternity::game {
bool VerticalCombatMobStateServices::long_corpse_delay(const MobRuntime& mob){
 const auto record=mob_definition_record(mob);if(record.size()<0x70)throw std::logic_error("Combat mob definition record too short");
 for(std::size_t i=0x6c;i<0x70;++i)if(record[i]!=std::byte{0})return true;
 return false;
}
bool VerticalCombatMobStateServices::damage_logging_enabled(const MobRuntime& mob){return mob.lua_ai!=0 || (mob.respawn && mob.respawn->both);}
}
