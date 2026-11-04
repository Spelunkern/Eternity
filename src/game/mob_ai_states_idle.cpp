#include "eternity/game/mob_ai_states_idle.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <limits>
namespace eternity::game {
namespace {
float distance(UserPosition a,UserPosition b) {
    const auto x=static_cast<double>(static_cast<float>(a.x-b.x));
    const auto z=static_cast<double>(static_cast<float>(a.z-b.z));
    return static_cast<float>(std::sqrt(x*x+z*z));
}
std::int32_t integer(float value) {
    const double truncated=std::trunc(static_cast<double>(value));
    if(!std::isfinite(truncated) || truncated<INT32_MIN || truncated>INT32_MAX) return INT32_MIN;
    return static_cast<std::int32_t>(truncated);
}
}
void update_mob_ai_idle(MobRuntime& mob,const MobAiIdleProjection& inputs,MobAiIdleServices& services) {
    const auto now=services.tick();mob.current_time=now;
    bool find=true;
    if(mob.next_new_target!=0) {
        if(now<mob.next_new_target) find=false;
        else mob.next_new_target=0;
    }
    if(find && services.find_target(mob)) {services.dispatch(mob,0,mob.target_id);return;}
    if(inputs.obelisk || mob.next_move>mob.current_time) return;
    const auto home_distance=distance(mob.position,mob.respawn_position);
    if((inputs.chase_path_present && home_distance>0.5F) || home_distance>10.0F) {
        services.dispatch(mob,4,0);return;
    }
    if(inputs.ai==4 || mob.stop!=0 || mob.sleep!=0 || mob.stun!=0 || inputs.move_speed==0 || inputs.normal_step==0 || mob.hold_position!=0) {
        const auto delay=services.random()%30000;
        mob.next_move=mob.current_time+10000U+static_cast<std::uint32_t>(delay);return;
    }
    const auto angle_random=services.random()%10000;
    const auto extra=(services.random()%500)/100;
    constexpr std::array<std::int32_t,5> speeds{-4,-2,0,2,4}; // 00542538
    const auto step=static_cast<float>(speeds[static_cast<std::size_t>(std::clamp(mob.move_speed,0,4))]+inputs.normal_step);
    const float length=step+static_cast<float>(extra);
    const float radians=static_cast<float>(static_cast<double>(static_cast<float>(angle_random))*std::bit_cast<float>(0x3c8efa35U));
    const float sine=static_cast<float>(std::sin(static_cast<double>(radians)));
    const float cosine=static_cast<float>(std::cos(static_cast<double>(radians)));
    const float rotated_x=static_cast<float>(static_cast<double>(cosine)*0.0-static_cast<double>(sine)*length);
    const float rotated_z=static_cast<float>(static_cast<double>(sine)*0.0+static_cast<double>(cosine)*length);
    const UserPosition candidate{rotated_x+mob.position.x,0.0F+mob.position.y,rotated_z+mob.position.z};
    if(inputs.zone_available && distance(candidate,mob.respawn_position)<10.0F &&
       services.enable_line(mob,integer(mob.position.x),integer(mob.position.z),integer(candidate.x),integer(candidate.z))) services.move(mob,candidate,0);
    const auto delay=services.random()%30000;
    mob.next_move=mob.current_time+10000U+inputs.normal_time+static_cast<std::uint32_t>(delay);
}
}
