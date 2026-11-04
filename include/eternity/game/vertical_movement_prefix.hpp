#pragma once
#include "eternity/game/vertical_movement_dispatch.hpp"
namespace eternity::game {
// Required real StatusStand, direction owner and PSendViewAction transports.
// The actor, admission and zone must remain pinned and serialized through calls.
class VerticalMovementPrefixServices {
public:
    virtual ~VerticalMovementPrefixServices()=default;
    virtual void status_stand(Actor&)=0;
    virtual std::uint16_t direction(const Actor&)=0;
    virtual void send_view_action(Actor&,const net::Packet&)=0;
};
enum class VerticalMovementPrefixResult {life_blocked,distance_corrected,check_required};
// PacketPC 0046a286..0046a36d. Requires real StatusStand service, not an
// assumed successful stand. Malformed/unadmitted inputs fail before side effects.
VerticalMovementPrefixResult vertical_movement_prefix(Actor&,RuntimeZone&,const ZoneEntryState&,
    std::span<const std::uint8_t>,VerticalMovementPrefixServices&);
// PacketPC 0046a3d5..0046a476. Call only after the real speed check rejected;
// its Reset has already stored the original back position in this actor's check.
// Rewrites input coordinates in place, then real attack stop, 020b and self 0502.
void vertical_movement_speed_correction(Actor&,RuntimeZone&,const ZoneEntryState&,
    std::span<std::uint8_t>,VerticalMovementPrefixServices&);
}
