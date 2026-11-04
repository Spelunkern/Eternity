#include "eternity/game/mob_drops_range.hpp"
#include <cmath>

namespace eternity::game {
bool mob_drop_zone_enabled(std::uint32_t zone,const MobLootUser& credited,const MobLootUser& candidate) {
    return credited.state==4 && credited.map==zone && candidate.state==4 && candidate.map==credited.map && candidate.instance==credited.instance;
}
namespace {
bool distance_enabled(const MobLootUser& credited,const MobLootUser& candidate,std::int32_t radius) {
    const float dx=candidate.x-credited.x,dz=candidate.z-credited.z;
    // Separate double stores retain the original x87 53-bit multiplication/add
    // boundaries and prevent fused multiply-add from changing a boundary case.
    const volatile double xx=static_cast<double>(dx)*dx,zz=static_cast<double>(dz)*dz;
    const auto distance=static_cast<float>(std::sqrt(xx+zz));
    return !(distance>static_cast<float>(radius));
}
}
bool mob_drop_range_enabled(std::uint32_t zone,const MobLootUser& credited,const MobLootUser& candidate,std::int32_t radius) {
    if(credited.state!=4 || credited.map!=zone || candidate.dead==1 || candidate.state!=4 || candidate.map!=credited.map || candidate.instance!=credited.instance) return false;
    return distance_enabled(credited,candidate,radius);
}
bool mob_drop_money_range_enabled(const MobLootUser& credited,const MobLootUser& candidate) {
    if(candidate.dead==1 || candidate.state!=4 || candidate.map!=credited.map || candidate.instance!=credited.instance) return false;
    return distance_enabled(credited,candidate,100);
}
}
