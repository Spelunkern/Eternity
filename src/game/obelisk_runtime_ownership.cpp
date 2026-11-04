#include "eternity/game/obelisk_runtime_ownership.hpp"
#include <bit>
#include <stdexcept>

namespace eternity::game {
void restore_obelisk_ownership(std::span<const std::span<ObeliskRuntime>> maps,
    std::span<ObeliskRuntimePortalOwnership> portals,std::uint32_t base_id,
    std::uint32_t packed_country,std::uint32_t change_time,ObeliskRuntimeOwnershipClock& clock) {
    if((base_id&0x10000000U)!=0) {
        for(auto& portal:portals) {
            if(portal.base_id!=base_id) continue;
            if(!portal.country) return;
            if(!portal.open) throw std::invalid_argument("Portal ownership requires open flags");
            *portal.country=std::bit_cast<std::int32_t>(packed_country);
            (*portal.open)[0]=packed_country==0 || packed_country==1;
            (*portal.open)[1]=packed_country==0 || packed_country==2;
            return;
        }
        return;
    }
    for(auto map:maps) for(auto& obelisk:map) {
        if(obelisk.base_id!=base_id) continue;
        std::uint32_t remaining=0;
        if(change_time!=0) {
            const auto now=clock.packed_now();
            const auto seconds=clock.seconds_between(now,change_time);
            // Original zero-extends SubTimeSecond, subtracts from 86400 in
            // 64 bits, multiplies by 1000, then clamps a negative product.
            if(seconds<=86400U) remaining=(86400U-seconds)*1000U;
        }
        const auto country=packed_country&15U;
        auto last=static_cast<std::int32_t>((packed_country>>4)&15U)-1;
        if(last<0 || country!=2) last=static_cast<std::int32_t>(country);
        obelisk.country=static_cast<std::int32_t>(country);obelisk.last_destroy_country=last;
        obelisk.reset_time=remaining;obelisk.change_time=change_time;
        return;
    }
}
}
