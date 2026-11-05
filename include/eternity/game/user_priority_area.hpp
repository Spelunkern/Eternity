#pragma once
#include "eternity/game/user_priority_cell.hpp"
#include <algorithm>
#include <limits>

namespace eternity::game {
class UserPriorityAreaServices:public UserPriorityCellServices {
public:
    virtual std::unique_ptr<UserViewBuffer> allocate(const net::Packet&)=0;
};
// CZone::PSendRange 00421220 / PSendRing 004213d0. The rectangle is
// calculated before allocation from the then-current origin; individual
// recipient distances sample the live origin again. Every pass separately
// allocates one retained buffer and visits cells in Z-major order.
inline void user_priority_area(ZoneUsers& zone,std::int32_t width,std::int32_t height,const net::Packet& packet,
    float inner,float outer,bool ring,std::uint32_t source,std::uint32_t extra,std::uint8_t priority,UserPriorityAreaServices& services) {
    if(width<=0 || height<=0)throw std::invalid_argument("Invalid priority area geometry");
    const auto origin=services.origin();
    const auto coordinate=[&](float value,bool plus) {
        const auto result=std::trunc((static_cast<double>(value)+(plus?static_cast<double>(outer):-static_cast<double>(outer)))/40.0);
        if(!std::isfinite(result) || result<INT32_MIN || result>INT32_MAX)return INT32_MIN;
        return static_cast<std::int32_t>(result);
    };
    const auto min_x=std::max(0,coordinate(origin.x,false)),max_x=std::min(width-1,coordinate(origin.x,true));
    const auto min_z=std::max(0,coordinate(origin.z,false)),max_z=std::min(height-1,coordinate(origin.z,true));
    auto buffer=services.allocate(packet);if(!buffer)return;
    for(std::int64_t z=min_z;z<=max_z;++z)for(std::int64_t x=min_x;x<=max_x;++x) {
        const CellCoordinates cell{static_cast<std::int32_t>(x),static_cast<std::int32_t>(z)};
        if(ring)user_priority_cell_ring(zone,cell,*buffer,inner,outer,source,extra,priority,services);
        else user_priority_cell(zone,cell,*buffer,outer,source,extra,priority,services);
    }
}
}
