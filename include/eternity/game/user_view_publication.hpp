#pragma once
#include "eternity/game/user_view_difference.hpp"
#include <memory>

namespace eternity::game {
enum class UserViewBufferKind {leave,appearance,equipment,shape};
struct UserViewBuffer {virtual ~UserViewBuffer() = default;};
// The provider owns actual packet construction, buffer lifetime, per-cell
// delivery and snapshots. A null buffer models original allocation failure.
// Calls must not throw; subjects/cells survive synchronous reentry. Shape is
// sampled after the first two passes, as in 0041f73e, not at function entry.
class UserViewPublicationServices {
public:
    virtual ~UserViewPublicationServices() = default;
    virtual std::unique_ptr<UserViewBuffer> allocate(UserViewBufferKind)=0;
    virtual void broadcast(CellCoordinates,UserViewBuffer&)=0;
    virtual void leave_snapshot(CellCoordinates)=0;
    virtual void enter_snapshot(CellCoordinates)=0;
    virtual bool publish_shape()=0;
};
inline void publish_user_view_difference(std::int32_t width,std::int32_t height,CellCoordinates old_cell,
    CellCoordinates new_cell,UserViewPublicationServices& services) {
    // Validate before side effects; original callers supply valid grid cells.
    if(width<=0 || height<=0 || old_cell.x<0 || old_cell.z<0 || new_cell.x<0 || new_cell.z<0 ||
       old_cell.x>=width || new_cell.x>=width || old_cell.z>=height || new_cell.z>=height)
        throw std::invalid_argument("User view requires valid zone cells");
    auto leave=services.allocate(UserViewBufferKind::leave);
    if(leave)user_view_difference(width,height,old_cell,new_cell,[&](auto cell){services.broadcast(cell,*leave);services.leave_snapshot(cell);},[](auto){});
    leave.reset();
    auto appearance=services.allocate(UserViewBufferKind::appearance);
    auto equipment=services.allocate(UserViewBufferKind::equipment);
    if(appearance && equipment)user_view_difference(width,height,old_cell,new_cell,[](auto){},[&](auto cell){services.broadcast(cell,*appearance);services.broadcast(cell,*equipment);services.enter_snapshot(cell);});
    appearance.reset();equipment.reset();
    if(!services.publish_shape())return;
    auto shape=services.allocate(UserViewBufferKind::shape);
    if(shape)user_view_difference(width,height,old_cell,new_cell,[](auto){},[&](auto cell){services.broadcast(cell,*shape);});
}
}
