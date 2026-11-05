#include "eternity/game/vertical_combat_mob_position.hpp"
#include "eternity/game/vertical_combat_success.hpp"
#include "eternity/game/vertical_movement.hpp"
#include <cmath>
#include <stdexcept>
namespace eternity::game {
namespace {
std::uint32_t word(std::span<const std::byte> record,std::size_t at){
 if(record.size()<at+4)throw std::logic_error("Combat mob definition record too short");
 std::uint32_t result=0;for(unsigned i=0;i<4;++i)result|=std::uint32_t(std::to_integer<unsigned char>(record[at+i]))<<(8*i);return result;
}
float finite(float f){if(!std::isfinite(f))throw std::logic_error("Mob position nonfinite arithmetic is not recovered");return f;}
}
UserPosition VerticalCombatMobPositionServices::mob_current_position(MobRuntime& mob){
 const auto now=tick_ms();
 const bool moving=mob_secondary_state(mob)==1;
 const auto status=moving?0:(mob.state_stack?(mob.state_stack->empty()?0:mob.state_stack->back()):mob.status);
 const auto definition=mob_definition_record(mob);
 const auto duration=word(definition,(moving||status==3)?0x64:0x5c);
 const auto start=mob.previous_move_time;
 if(now>start+duration)return mob.position;
 if(!duration)throw std::logic_error("Mob position division by zero");
 vertical_combat_require_float_environment();(void)vertical_movement_planar_distance({},{});
 const float steps=static_cast<float>((now-start)/duration);
 const float travel=finite(static_cast<float>(static_cast<double>(steps)*finite(mob.move_range)));
 const float x=finite(static_cast<float>(static_cast<double>(travel)*finite(mob.move_direction.x)));
 const float y=finite(static_cast<float>(static_cast<double>(travel)*finite(mob.move_direction.y)));
 const float z=finite(static_cast<float>(static_cast<double>(travel)*finite(mob.move_direction.z)));
 return {finite(static_cast<float>(static_cast<double>(x)+finite(mob.previous_position.x))),
         finite(static_cast<float>(static_cast<double>(y)+finite(mob.previous_position.y))),
         finite(static_cast<float>(static_cast<double>(z)+finite(mob.previous_position.z)))};
}
std::uint8_t VerticalCombatMobPositionServices::mob_size(const MobRuntime& mob){
 const auto record=mob_definition_record(mob);if(record.size()<=0x3f)throw std::logic_error("Combat mob definition record too short");return std::to_integer<std::uint8_t>(record[0x3f]);
}
}
