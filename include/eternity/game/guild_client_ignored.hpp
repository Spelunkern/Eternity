#pragma once
#include <cstdint>
#include <span>

namespace eternity::game {
// ps_game.exe CUser::PacketGuild 004764a0, switch 004764f2, shared
// return 0047768c. Unrecognized and server-only commands have no effect.
inline bool guild_client_ignored(std::span<const std::uint8_t> packet) noexcept {
    if(packet.size()<2 || packet[1]!=0x0d)return false;
    const auto op=packet[0];
    if(op>=0x03 && op<=0x0a)return false;
    switch(op) {
    case 0x0c:case 0x20:case 0x21:case 0x22:case 0x23:case 0x25:case 0x34:case 0x35:return false;
    default:return true;
    }
}
}
