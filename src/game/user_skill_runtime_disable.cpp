#include "eternity/game/user_skill_runtime_disable.hpp"
#include <bit>
namespace eternity::game {
void user_skill_runtime_keep_tail(Actor& actor,bool periodic_damage_seen){
    auto& status=actor.status();
    if(status.casting!=0){
        const auto now=actor.tick_ms();
        // Re-read live DWORD after clock; equality remains blocked. A callback
        // clearing it can still cause the original speed-send branch to execute.
        if(std::bit_cast<std::uint32_t>(status.casting)<now){status.casting=0;actor.speed_changed(status);}
    }
    // Live mode is checked AFTER speed delivery, once for the two cleanup calls.
    if(periodic_damage_seen&&actor.attack_state().mode!=1){actor.cancel_shape_on_attack();actor.remove_sleep();}
}
}
