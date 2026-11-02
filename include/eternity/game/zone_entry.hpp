#pragma once
#include "eternity/game/actor.hpp"
#include "eternity/game/zones.hpp"

namespace eternity::game {
struct ZoneEntryMap {
    std::int32_t size=0,width=0,height=0;
    std::uint16_t id=0;
    std::uint32_t dungeon=0; // Source compares the map DWORD to exactly 1.
};
struct ZoneEntryState {
    std::uint32_t character_id=0;
    std::uint8_t permission=255;
    std::uint16_t map=0,direction=0;
    UserPosition position;
    CellCoordinates cell;
    std::int32_t vehicle_mode=0,guild_user_level=0;
    std::uint32_t vehicle_partner=0;
    bool bound=false;
};
struct ZoneEntryPorts {
    // One serialized membership operation implements consecutive CCell::EnterUser
    // and CZone::AddUser. All delivery callbacks are required and do not throw.
    std::function<void(CellCoordinates)> attach,cell_info;
    std::function<void(CellCoordinates,net::Packet)> send_view;
    std::function<std::int32_t()> shape;
    std::function<void()> send_shape,send_speed,weather,obelisks,entered,diagnostic;
    std::function<void(std::uint32_t)> close;
};
// ps_game.exe CZone::EnterUser 00415f80..0041619e.
bool enter_zone(ZoneEntryState&,const ZoneEntryMap&,const ZoneEntryPorts&);
struct ZoneActorEntryServices {
    std::function<void(CellCoordinates,net::Packet)> send_view;
    std::function<void(CellCoordinates)> cell_info;
    std::function<void()> obelisks,entered,diagnostic;
    std::function<void(std::uint32_t)> close;
};
// Binds actual actor membership, shape/speed and zone weather. Scene/transport,
// cell entity snapshots, obelisks and EnterZone lifecycle remain caller services.
bool enter_zone_actor(RuntimeZone&,Actor&,ZoneEntryState&,const ZoneActorEntryServices&);
}
