#pragma once
#include "eternity/game/actor.hpp"
#include <bit>
#include <optional>

namespace eternity::game {
// ps_game.exe CZone::SendMoveUser 0041f404 and 0041f73e.
inline net::Packet user_view_leave_packet(std::uint32_t id) {
    net::Packet packet{2,2};for(unsigned shift=0;shift<32;shift+=8)packet.push_back(static_cast<std::uint8_t>(id>>shift));return packet;
}
// The zone owner supplies the current live position, direction, permission and
// guild level; these must be sampled after preceding leave callbacks.
inline net::Packet user_view_appearance_packet(const Actor& actor,std::uint8_t permission,
    std::uint16_t direction,UserPosition position,std::int32_t guild_level) {
    net::Packet packet{1,2};const auto append=[&](std::uint32_t value,unsigned count){for(unsigned i=0;i<count;++i)packet.push_back(static_cast<std::uint8_t>(value>>(8*i)));};
    append(actor.id(),4);packet.push_back(permission<=10?2:0);append(direction,2);
    append(std::bit_cast<std::uint32_t>(position.x),4);append(std::bit_cast<std::uint32_t>(position.y),4);append(std::bit_cast<std::uint32_t>(position.z),4);
    append(std::bit_cast<std::uint32_t>(guild_level),4);append(actor.appearance().vehicle_mode==3?actor.appearance().vehicle_partner:0,4);return packet;
}
// The original tests the full DWORD vehicle sum before writing its low byte.
// A nonzero sum divisible by 256 therefore still emits a zero-valued 051d.
inline std::optional<net::Packet> user_view_shape_packet(const Actor& actor) {
    const auto& appearance=actor.appearance();std::uint32_t shape=13;
    if(appearance.visible) {
        shape=appearance.vehicle_mode==2 ? static_cast<std::uint32_t>(appearance.vehicle_shape)+static_cast<std::uint32_t>(appearance.vehicle_addition) : actor.status().shape;
        if(!shape)return {};
    }
    net::Packet packet{0x1d,5};const auto id=actor.id();for(unsigned shift=0;shift<32;shift+=8)packet.push_back(static_cast<std::uint8_t>(id>>shift));
    packet.push_back(static_cast<std::uint8_t>(shape));return packet;
}
}
