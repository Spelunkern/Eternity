#include "eternity/game/vertical_movement.hpp"
#include <cmath>
#include <cfenv>
#include <limits>
#if defined(__x86_64__) && defined(__SSE2__)
#include <xmmintrin.h>
#endif
#include <stdexcept>
namespace eternity::game {
namespace {
void require_environment() {
#if defined(__x86_64__) && defined(__SSE2__)
    // SSE rounding, flush-to-zero and denormals-are-zero are independent of x87.
    constexpr unsigned incompatible=0x6000U | 0x8000U | 0x0040U;
    if(std::fegetround()!=FE_TONEAREST || (_mm_getcsr() & incompatible)!=0)
        throw std::logic_error("Movement check requires nearest rounding and gradual underflow");
#else
    throw std::logic_error("Movement check floating environment is supported only on x86-64 SSE2");
#endif
    static_assert(std::numeric_limits<float>::is_iec559 && std::numeric_limits<double>::is_iec559);
}
bool finite(UserPosition p) {return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);}
void require_finite(const ZoneMovementCheck& s,UserPosition p,float speed) {
    if(!finite(s.back) || !finite(s.previous) || !finite(p) || !std::isfinite(speed) ||
       !std::isfinite(s.total_distance) || !std::isfinite(s.maximum_speed))
        throw std::logic_error("Movement check nonfinite operands are not recovered");
}
}
float vertical_movement_planar_distance(UserPosition a,UserPosition b) {
    require_environment();
    if(!finite(a) || !finite(b)) throw std::logic_error("Movement distance nonfinite operands are not recovered");
    const float dx=static_cast<float>(static_cast<double>(a.x)-b.x);
    const float dz=static_cast<float>(static_cast<double>(a.z)-b.z);
    const float distance=static_cast<float>(std::sqrt(static_cast<double>(dx)*dx+static_cast<double>(dz)*dz));
    if(!std::isfinite(dx) || !std::isfinite(dz) || !std::isfinite(distance))
        throw std::logic_error("Movement distance overflow is not recovered");
    return distance;
}
bool vertical_movement_update(ZoneMovementCheck& s, UserPosition p, float speed,
                              const std::function<std::uint32_t()>& tick) {
    if(!tick) throw std::invalid_argument("Movement check requires a clock");
    require_environment();
    require_finite(s,p,speed);
    const auto now=tick();
    require_environment();
    require_finite(s,p,speed);
    const float dx=static_cast<float>(static_cast<double>(s.previous.x)-p.x);
    const float dz=static_cast<float>(static_cast<double>(s.previous.z)-p.z);
    const float distance=static_cast<float>(std::sqrt(static_cast<double>(dx)*dx+static_cast<double>(dz)*dz));
    const float total=static_cast<float>(static_cast<double>(distance)+s.total_distance);
    if(!std::isfinite(dx) || !std::isfinite(dz) || !std::isfinite(distance) || !std::isfinite(total))
        throw std::logic_error("Movement check overflow propagation is not recovered");
    s.total_distance=total;
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
