#pragma once
#include "eternity/game/cell_snapshot.hpp"
#include "eternity/game/zone_entry.hpp"
#include <algorithm>
#include <stdexcept>

namespace eternity::game {
// CZone::SendCellInfo 0041f290: clipped radius two, Z outer and X inner.
// Dimensions, snapshot and all its registry owners must remain live through
// entry callbacks. The caller binds snapshots to the entering user's channel.
template<class Visitor>
void visit_cell_entry_view(std::int32_t width,std::int32_t height,CellCoordinates center,Visitor&& visitor) {
    if(width<=0 || height<=0 || center.x<0 || center.z<0 || center.x>=width || center.z>=height)
        throw std::invalid_argument("Invalid entry-view geometry");
    const auto min_x=std::max<std::int64_t>(0,static_cast<std::int64_t>(center.x)-2);
    const auto max_x=std::min<std::int64_t>(width-1,static_cast<std::int64_t>(center.x)+2);
    const auto min_z=std::max<std::int64_t>(0,static_cast<std::int64_t>(center.z)-2);
    const auto max_z=std::min<std::int64_t>(height-1,static_cast<std::int64_t>(center.z)+2);
    for(auto z=min_z;z<=max_z;++z)for(auto x=min_x;x<=max_x;++x)
        visitor(CellCoordinates{static_cast<std::int32_t>(x),static_cast<std::int32_t>(z)});
}
inline ZoneActorEntryServices bind_entry_cell_snapshots(ZoneActorEntryServices services,
    std::int32_t width,std::int32_t height,CellSnapshot& snapshot) {
    services.cell_info=[width,height,&snapshot](CellCoordinates center) {
        visit_cell_entry_view(width,height,center,[&](CellCoordinates cell){snapshot.enter(cell);});
    };
    return services;
}
}
