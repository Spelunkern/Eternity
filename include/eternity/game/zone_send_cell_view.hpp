#pragma once
#include "eternity/game/cell_entry_view.hpp"
#include "eternity/game/user_view_actor_publication.hpp"

namespace eternity::game {
// CZone::SendView 004208c0 allocates one shared packet buffer, visits the
// clipped two-cell radius in row-major order and drops its own buffer reference.
// A failed allocation silently skips all cells. Recipient retention, live socket
// filtering and enqueue ownership are provided by the actual transport.
inline void zone_send_cell_view(std::int32_t width,std::int32_t height,CellCoordinates center,
    const net::Packet& packet,UserViewActorTransport& transport) {
    if(width<=0 || height<=0 || center.x<0 || center.z<0 || center.x>=width || center.z>=height)
        throw std::invalid_argument("Invalid zone view geometry");
    auto buffer=transport.allocate(packet);
    if(!buffer)return;
    visit_cell_entry_view(width,height,center,[&](CellCoordinates cell){transport.broadcast(cell,*buffer);});
}
}
