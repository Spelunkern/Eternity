#pragma once
#include "eternity/game/obelisk_runtime.hpp"

namespace eternity::game {
// Stable live projection of stMapPortalLink (0x84) and its target portal/map.
struct ObeliskRuntimePortalLink {
    std::uint32_t id=0,base_id=0;
    std::uint16_t portal_id=0;
    std::int32_t* country=nullptr; // Null projects missing pPortal.
    bool zone_present=false;
    std::array<bool,2> open{};
    std::array<std::int32_t,2> counts{};
    // Flattened original pLink[2][4], indexed by country*4+n.
    std::array<const ObeliskRuntime*,8> links{};
};
class ObeliskRuntimePortalWorld {
public:
    virtual ~ObeliskRuntimePortalWorld()=default;
    virtual void database(const net::Packet&)=0;
    virtual void map_broadcast(std::size_t link,const net::Packet&)=0;
    virtual void broadcast(const net::Packet&)=0;
};
// ps_game.exe SendPortalLinkChange 0042b2c0. Caller supplies a valid portal.
void publish_obelisk_portal_change(std::span<ObeliskRuntimePortalLink>,std::size_t,ObeliskRuntimePortalWorld&);
// SendDBPortalLinkList 0042b460: persisted initial rows use country=3/time=0,
// regardless of live portal state. Base IDs are reread after full-chunk callbacks.
void publish_obelisk_portal_database_list(std::span<const ObeliskRuntimePortalLink>,ObeliskRuntimePortalWorld&);
// UpdatePortalLink 0042a000. Original stack has exactly 12 neighbor flags.
// Providers may mutate live fields but must retain records and pointed-to storage.
void update_obelisk_portal_links(std::span<ObeliskRuntimePortalLink>,ObeliskRuntimePortalWorld&);
}
