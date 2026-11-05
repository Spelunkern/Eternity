#pragma once
#include "eternity/game/guild_catalog.hpp"
#include "eternity/game/zone_leave_guild_creation.hpp"

namespace eternity::game {
// PacketGuild 00476e32..00476e82 checks CUser+175c before looking up
// a target. The same service owns that field during LeaveZone cleanup.
inline EntryCatalogUpdate guild_client_application_creation(GuildUser& user,Actor& actor,ZoneGuildCreationServices& creation,
    std::span<const std::uint8_t> packet,const std::function<void(GuildUser&,net::Packet)>& send) {
    if(packet.size()<2)return EntryCatalogUpdate::rejected;
    if(packet[0]!=7 || packet[1]!=13)return EntryCatalogUpdate::unsupported;
    if(packet.size()<6)return EntryCatalogUpdate::rejected;
    if(!creation.current(actor))return EntryCatalogUpdate::unsupported;
    if(!send)throw std::invalid_argument("Missing guild application client transport");
    send(user,net::Packet{7,13,0});return EntryCatalogUpdate::applied;
}
}
