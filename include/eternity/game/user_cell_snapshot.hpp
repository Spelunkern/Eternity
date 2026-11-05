#pragma once
#include "eternity/game/user_view_actor_publication.hpp"
#include <algorithm>
#include <array>

namespace eternity::game {
// The original cell-entry 051c contains four bytes not written by the original
// routine. The owner must supply a policy for these indeterminate bytes; they
// are not assumed to be the actor ID. Calls are serialized by the world owner.
class UserCellSnapshotServices {
public:
    virtual ~UserCellSnapshotServices()=default;
    virtual std::shared_ptr<void> retain(Actor&)=0;
    virtual UserViewLiveAppearance appearance(const Actor&)=0;
    virtual std::array<std::uint8_t,4> speed_stack_bytes()=0;
    virtual void send_self(const net::Packet&)=0;
};
inline void user_cell_snapshot_enter(ZoneUsers& zone,CellCoordinates cell,UserCellSnapshotServices& services) {
    // CCell::SendCellInfoEnter 00423aa0, user block 00423ab7..00423cc1.
    const auto stack=services.speed_stack_bytes();
    zone.visit_cell(cell.x,cell.z,[&](Actor& actor,UserPosition) {
        auto pin=services.retain(actor);if(!pin)throw std::logic_error("Cannot retain snapshot actor");
        const auto live=services.appearance(actor);
        services.send_self(user_view_appearance_packet(actor,live.permission,live.direction,live.position,live.guild_level));
        if(auto shape=user_view_shape_packet(actor))services.send_self(*shape);
        auto speed=character_speed(0,actor.status());
        std::copy(stack.begin(),stack.end(),speed.begin()+2);services.send_self(speed);
        return false;
    });
}
inline void user_cell_snapshot_leave(ZoneUsers& zone,CellCoordinates cell,UserCellSnapshotServices& services) {
    // CCell::SendCellInfoLeave 00424130, user block 0042413a..004241aa.
    zone.visit_cell(cell.x,cell.z,[&](Actor& actor,UserPosition) {
        auto pin=services.retain(actor);if(!pin)throw std::logic_error("Cannot retain snapshot actor");
        services.send_self(user_view_leave_packet(actor.id()));return false;
    });
}
}
