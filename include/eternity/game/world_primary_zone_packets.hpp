#pragma once
#include "eternity/game/world_admission.hpp"
#include "eternity/game/vertical_world.hpp"
#include "eternity/game/zone_users.hpp"

namespace eternity::game {
// Supplies the real session/actor owners used by CUser packet dispatch and
// movement reset; retain must pin both actor and session through a callback.
class PrimaryZonePacketServices {
public:
    virtual ~PrimaryZonePacketServices()=default;
    virtual std::shared_ptr<void> retain_packet_actor(Actor&)=0;
    virtual void update_actor_packets(RuntimeZone&,Actor&)=0;
    virtual std::uint32_t packet_tick(RuntimeZone&)=0;
    virtual void update_actor_reset_position(RuntimeZone&,Actor&,std::uint32_t)=0;
};
// ps_game.exe CZone::UpdatePackets 004215e0: waiting users first, then the
// in-zone packet and movement reset traversals, with a fresh clock sample
// between the latter two. The caller owns world/zone serialization.
inline bool update_primary_zone_packets(VerticalWorld& world,WorldAdmission& admission,
    RuntimeZone& zone,PrimaryZonePacketServices& services,const std::function<void(RuntimeZone&)>& update_waiting={}) {
    if(!world.running()) return false;
    if(!zone.users) throw std::logic_error("Primary zone has no retained user owner");
    if(update_waiting)update_waiting(zone);else admission.update_zone_wait_packets(zone);
    if(!world.running()) return false;
    auto retain=[&](Actor& actor){return services.retain_packet_actor(actor);};
    zone.users->visit_all(retain,[&](Actor& actor){
        if(world.running()) services.update_actor_packets(zone,actor);
    });
    if(!world.running()) return false;
    const auto tick=services.packet_tick(zone);
    if(!world.running()) return false;
    zone.users->visit_all(retain,[&](Actor& actor){
        if(world.running()) services.update_actor_reset_position(zone,actor,tick);
    });
    return world.running();
}
}
