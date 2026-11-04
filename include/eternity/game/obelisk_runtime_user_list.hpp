#pragma once
#include "eternity/game/obelisk_runtime_portal.hpp"
namespace eternity::game {
// Original target-map pointer identity and live portal position. All borrowed
// storage remains stable during user transport callbacks; values may change.
struct ObeliskRuntimePortalScene {
    const void* map=nullptr;
    const ObeliskRuntimePortalLink* link=nullptr;
    const UserPosition* position=nullptr;
};
class ObeliskRuntimeUserPublication {
public:
    virtual ~ObeliskRuntimeUserPublication()=default;
    virtual void send_user(const net::Packet&)=0;
};
// ps_game.exe CMap::SendObeliskList 0042b040. Ordinary rows precede matching
// portal rows, sharing the same partial chunk. No calling-map zone guard.
void publish_obelisk_user_list(const void* map,std::span<const ObeliskRuntime>,
    std::span<const ObeliskRuntimePortalScene>,ObeliskRuntimeUserPublication&);
}
