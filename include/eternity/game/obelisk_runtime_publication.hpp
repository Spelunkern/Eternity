#pragma once
#include "eternity/game/obelisk_runtime.hpp"

namespace eternity::game {
class ObeliskRuntimePublication {
public:
    virtual ~ObeliskRuntimePublication()=default;
    virtual void zone_packet(const net::Packet&)=0;
    virtual void database_packet(const net::Packet&)=0;
};
// ps_game.exe SendDBObeliskChange 0042b260. Also the exact inline persistence
// record used by Destroy/Reset. Countries are packed with original byte wrapping.
net::Packet obelisk_database_change_packet(const ObeliskRuntime&);
// CMap::SendObeliskChange 0042b200: only the zone guard suppresses this send;
// reset_send is checked by the caller, not this publication function.
void publish_obelisk_change(const ObeliskRuntimeMap&,const ObeliskRuntime&,ObeliskRuntimePublication&);
void publish_obelisk_database_change(const ObeliskRuntime&,ObeliskRuntimePublication&);
// CMap::SendDBObeliskList 0042b370. Records stay live across callbacks; providers
// may update values but must not resize, move or destroy the projected storage.
void publish_obelisk_database_list(std::span<const ObeliskRuntime>,ObeliskRuntimePublication&);
}
