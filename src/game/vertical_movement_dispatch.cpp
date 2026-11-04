#include "eternity/game/vertical_movement_dispatch.hpp"
#include <bit>
#include <stdexcept>
namespace eternity::game {
namespace {
std::uint32_t word(std::span<const std::uint8_t> p,std::size_t offset,unsigned bytes) {
    std::uint32_t v=0;for(unsigned i=0;i<bytes;++i)v|=static_cast<std::uint32_t>(p[offset+i])<<(8*i);return v;
}
}
VerticalMovementPacketCheck vertical_movement_packet_check_after_gate(
    Actor& actor,RuntimeZone& zone,const ZoneEntryState& admission,ZoneMovementCheck& check,
    std::span<const std::uint8_t> p,float speed,const std::function<std::uint32_t()>& tick) {
    if(p.size()<17 || word(p,0,2)!=0x0501) throw std::invalid_argument("Movement check requires a complete 0501 packet");
    if(!zone.map || !zone.users || !admission.bound || admission.character_id!=actor.id() ||
       zone.map->settings.id<0 || zone.map->settings.id>65535 || admission.map!=zone.map->settings.id ||
       !actor.status().in_zone || zone.users->find_actor(actor.id())!=&actor ||
       &check!=&actor.movement_check())
        throw std::logic_error("Movement check requires the admitted actor's canonical zone and state");
    const auto position=zone.users->position(actor.id());
    if(!position) throw std::logic_error("Movement actor has no live zone position");
    VerticalMovementPacketCheck result{p[2],static_cast<std::uint16_t>(word(p,3,2)),
        {std::bit_cast<float>(word(p,5,4)),std::bit_cast<float>(word(p,9,4)),std::bit_cast<float>(word(p,13,4))},*position,false};
    result.accepted_by_speed_check=vertical_movement_update(check,result.destination,speed,tick);
    return result;
}
}
