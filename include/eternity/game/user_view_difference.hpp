#pragma once
#include "eternity/game/maps.hpp"
#include <algorithm>
#include <cstdint>
#include <stdexcept>

namespace eternity::game {
// CZone user-view transition 0041f350 visits old-only cells, then new-only
// cells, in row-major order within clipped two-cell rectangles. This computes
// only traversal; buffer allocation, recipient filtering, packet contents,
// cell snapshots and their additional passes remain the caller's responsibility.
// Callbacks must not change grid dimensions and the world owner must retain all
// referenced objects and serialize the traversal.
template<class Leave,class Enter>
void user_view_difference(std::int32_t width,std::int32_t height,CellCoordinates old_cell,
    CellCoordinates new_cell,Leave&& leave,Enter&& enter) {
    if(width<=0 || height<=0 || old_cell.x<0 || old_cell.z<0 || new_cell.x<0 || new_cell.z<0 ||
       old_cell.x>=width || new_cell.x>=width || old_cell.z>=height || new_cell.z>=height)
        throw std::invalid_argument("User view requires valid zone cells");
    struct Rect {std::int64_t min_x,max_x,min_z,max_z;};
    const auto bounds=[&](CellCoordinates c){return Rect{std::max<std::int64_t>(0,std::int64_t(c.x)-2),
        std::min<std::int64_t>(width-1,std::int64_t(c.x)+2),std::max<std::int64_t>(0,std::int64_t(c.z)-2),
        std::min<std::int64_t>(height-1,std::int64_t(c.z)+2)};};
    const auto old_view=bounds(old_cell),new_view=bounds(new_cell);
    const auto visit=[](Rect from,Rect excluded,auto&& callback){
        for(auto z=from.min_z;z<=from.max_z;++z)for(auto x=from.min_x;x<=from.max_x;++x)
            if(x<excluded.min_x || x>excluded.max_x || z<excluded.min_z || z>excluded.max_z)
                callback(CellCoordinates{static_cast<std::int32_t>(x),static_cast<std::int32_t>(z)});
    };
    visit(old_view,new_view,leave);visit(new_view,old_view,enter);
}
}
