#pragma once
#include "eternity/game/zone_entry.hpp"
#include "eternity/game/vertical_movement.hpp"
#include <span>
namespace eternity::game {
struct VerticalMovementPacketCheck {
    std::uint8_t motion;
    std::uint16_t direction;
    UserPosition destination;
    UserPosition position_before_check;
    bool accepted_by_speed_check;
};
// The 0501 speed-check stage of ps_game.exe CUser::PacketPC 0046a230,
// after StatusStand, the life gate, the 35-unit Distance2 gate and selection
// of the real vehicle/current speed. This is NOT the complete packet handler.
// Actor/zone/admission and packet storage must be serialized and retained across
// this call; tick must not reenter or change membership. The mutable check must
// alias actor.movement_check(), never a copy. An accepted result still requires
// MoveUserByPacket and all original continuation effects; a rejected result must
// execute the original attack-stop/correction sequence. This stage sends nothing.
VerticalMovementPacketCheck vertical_movement_packet_check_after_gate(
    Actor&,RuntimeZone&,const ZoneEntryState&,ZoneMovementCheck& canonical_check,
    std::span<const std::uint8_t> packet,float selected_speed,
    const std::function<std::uint32_t()>& tick);
}
