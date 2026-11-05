#pragma once
#include "eternity/game/guild_catalog.hpp"

namespace eternity::game {
// ps_game.exe CUser::PacketGuild 004764a0, 0d23 branch 00477185..004771c5;
// CGuildLock 0042b800 captures the guild lock then rereads CUser+1758. Both
// guild objects and this user must survive the synchronous send callback.
inline EntryCatalogUpdate guild_client_etin(GuildUser& user,GuildCatalog& catalog,std::span<const std::uint8_t> packet,
    const std::function<void(GuildUser&,net::Packet)>& send) {
    if(packet.size()<2)return EntryCatalogUpdate::rejected;
    if(packet[0]!=0x23 || packet[1]!=0x0d)return EntryCatalogUpdate::unsupported;
    if(!send)throw std::invalid_argument("Missing guild etin transport");
    auto* captured=user.guild;if(!captured)return EntryCatalogUpdate::applied;
    auto* locked=catalog.find_membership(captured);if(!locked)throw std::logic_error("Foreign captured guild owner");
    std::lock_guard guard(locked->mutex);
    auto* current=user.guild;if(!current)return EntryCatalogUpdate::applied;
    auto* guild=catalog.find_membership(current);if(!guild)throw std::logic_error("Foreign current guild owner");
    auto value=guild->metadata.etin;net::Packet response{0x23,0x0d};
    for(unsigned i=0;i<4;++i)response.push_back(static_cast<std::uint8_t>(value>>(8*i)));
    send(user,std::move(response));return EntryCatalogUpdate::applied;
}
}
