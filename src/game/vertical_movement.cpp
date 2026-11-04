#include "eternity/game/vertical_movement.hpp"
#include <cmath>
#include <stdexcept>
namespace eternity::game {
bool vertical_movement_update(ZoneMovementCheck& s, UserPosition p, float speed,
                              const std::function<std::uint32_t()>& tick) {
    if(!tick) throw std::invalid_argument("Movement check requires a clock");
    const auto now=tick();
    const float dx=static_cast<float>(static_cast<double>(s.previous.x)-p.x);
    const float dz=static_cast<float>(static_cast<double>(s.previous.z)-p.z);
    const float distance=static_cast<float>(std::sqrt(static_cast<double>(dx)*dx+static_cast<double>(dz)*dz));
    s.total_distance=static_cast<float>(static_cast<double>(distance)+s.total_distance);
    if(!(s.maximum_speed>speed)) s.maximum_speed=speed;
    s.previous=p;
    if(now<s.check_time) return true;
    const float elapsed=static_cast<float>(now-s.back_time);
    const float seconds=static_cast<float>(static_cast<double>(elapsed)/1000.0);
    const bool accepted=!(static_cast<double>(seconds)*s.maximum_speed+50.0 < s.total_distance);
    const auto reset_position=accepted?p:s.back;
    const auto reset_tick=tick();
    s.previous_time=reset_tick;s.previous=reset_position;
    s.total_distance=0;s.maximum_speed=0;s.back_time=reset_tick;
    s.check_time=reset_tick+5000U;s.back=reset_position;
    return accepted;
}
}
