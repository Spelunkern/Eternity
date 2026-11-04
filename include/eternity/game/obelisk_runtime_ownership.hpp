#pragma once
#include "eternity/game/obelisk_runtime.hpp"

namespace eternity::game {
// Explicit projection of stMapPortalLink's nBaseID, nullable pPortal->nCountry,
// and bOpen[2]. Backing storage must outlive this view and calendar callbacks.
struct ObeliskRuntimePortalOwnership {
    std::uint32_t base_id=0;
    std::int32_t* country=nullptr;
    std::array<bool,2>* open=nullptr;
};
class ObeliskRuntimeOwnershipClock {
public:
    virtual ~ObeliskRuntimeOwnershipClock()=default;
    // Original packed-calendar GetSystemTime 004cb8e0 / SubTimeSecond 004cb9d0.
    // Do not substitute Unix seconds or host calendar arithmetic implicitly.
    virtual std::uint32_t packed_now()=0;
    virtual std::uint32_t seconds_between(std::uint32_t now,std::uint32_t change)=0;
};
// CWorld::SetKeepObelisk 00413bd0. First match in map/record order; base IDs with
// bit 0x10000000 route exclusively to the first matching portal link. Calendar
// services run only for a found ordinary obelisk with nonzero change timestamp.
// Callbacks may mutate values but must retain all projected backing storage.
void restore_obelisk_ownership(std::span<const std::span<ObeliskRuntime>> maps,
    std::span<ObeliskRuntimePortalOwnership> portals,std::uint32_t base_id,
    std::uint32_t packed_country,std::uint32_t change_time,ObeliskRuntimeOwnershipClock&);
}
