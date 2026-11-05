#pragma once
#include "eternity/game/guild_catalog.hpp"
#include "eternity/game/actor.hpp"

namespace eternity::game {
// Complete deadline rejection prefix of ps_game.exe PacketGuild 00476dfc.
// Eligible requests return unsupported for the real application owner.
inline EntryCatalogUpdate guild_client_application_deadline(GuildUser& user,const Actor& actor,
    std::span<const std::uint8_t> packet,const std::function<void(GuildUser&,net::Packet)>& send) {
    if(packet.size()<2)return EntryCatalogUpdate::rejected;
    if(packet[0]!=7 || packet[1]!=13)return EntryCatalogUpdate::unsupported;
    if(packet.size()<6)return EntryCatalogUpdate::rejected;
    if(!user.join_deadline || actor.tick_ms()>=user.join_deadline)return EntryCatalogUpdate::unsupported;
    if(!send)throw std::invalid_argument("Missing guild application client transport");
    send(user,net::Packet{7,13,2});return EntryCatalogUpdate::applied;
}
}
