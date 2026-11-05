#pragma once
#include "eternity/game/user_lifecycle.hpp"
#include <span>

namespace eternity::game {
// ps_game.exe SendDBAgentUserLeave 0046c660. This is internal DB-agent wire,
// not a packet to the game client. Account ID is CUser+578c.
inline net::Packet vertical_database_disconnect_packet(std::uint32_t account) {
    net::Packet packet{2,3};
    for(unsigned i=0;i<4;++i)packet.push_back(static_cast<std::uint8_t>(account>>(i*8)));
    return packet;
}
// The caller owns DB-agent routing, the account lookup and lifecycle lifetime.
// Reject malformed/foreign packets before reaching the retained DB user.
inline bool vertical_database_disconnect_receive(std::span<const std::uint8_t> packet,
    const UserLifecycleState& state,UserLifecycle& lifecycle) {
    if(packet.size()!=6 || packet[0]!=2 || packet[1]!=3) return false;
    std::uint32_t account=0;for(unsigned i=0;i<4;++i)account|=std::uint32_t(packet[2+i])<<(i*8);
    if(account!=state.user_id)return false;
    lifecycle.disconnect_by_game_server();return true;
}
}
