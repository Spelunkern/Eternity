#pragma once
#include "eternity/game/guild_catalog.hpp"
#include <algorithm>
#include <functional>
#include <string>
#include <string_view>

namespace eternity::game {
// Live CUser fields captured at the log callback, including the original clock value.
// Coordinate DWORDs preserve their original float bit patterns.
struct GuildNpcLogUser {
    std::uint32_t account_id,character_id,field_13c,position_x,position_y,position_z,time;
    std::uint16_t field_136,field_164;
    std::string account_name,character_name;
};
// ps_game.exe CGuild::NpcUpgrade 0043096b and CUser::SetGameLogMain 0045b0d0.
inline net::Packet guild_npc_log_packet(const GuildNpcLogUser& user,const LoadedGuild& guild,std::uint16_t type,std::uint8_t level) {
    net::Packet packet(121,0);
    auto number=[&](std::size_t offset,std::uint32_t value,std::size_t size) {for(std::size_t i=0;i<size;++i)packet[offset+i]=static_cast<std::uint8_t>(value>>(i*8));};
    auto name=[&](std::size_t offset,std::size_t capacity,std::string_view value) {
        if(value.size()>=capacity || value.find('\0')!=value.npos)throw std::invalid_argument("Invalid guild game-log name");
        std::copy(value.begin(),value.end(),packet.begin()+offset);
    };
    number(0,0x0b09,2);number(2,user.account_id,4);name(6,19,user.account_name);
    number(0x19,user.character_id,4);name(0x1d,21,user.character_name);
    number(0x32,user.field_136,2);number(0x34,user.field_13c,4);number(0x38,user.field_164,2);
    number(0x3a,user.position_x,4);number(0x3e,user.position_y,4);number(0x42,user.position_z,4);number(0x46,user.time,4);
    number(0x4a,guild.metadata.listing.id,4);name(0x4e,32,guild.metadata.listing.name);
    number(0x6e,type,2);number(0x70,level,1);number(0x71,guild.metadata.etin,4);number(0x75,guild.metadata.keep_etin,4);
    return packet;
}
// The purchase record has the same common user and guild fields and no NPC tail.
// ps_game.exe CUser::PacketGuild 004772d8..00477325 (0d34).
inline net::Packet guild_house_log_packet(const GuildNpcLogUser& user,const LoadedGuild& guild) {
    auto packet=guild_npc_log_packet(user,guild,0,0);packet.resize(110);packet[0]=8;return packet;
}
// Both callbacks are mandatory and must be bound to the live session and actual
// game-log transport. Resolve the user only when invoked, after the DB send.
inline std::function<void(GuildUser&,const LoadedGuild&,std::uint16_t,std::uint8_t)> guild_npc_log_transport(
    std::function<GuildNpcLogUser(GuildUser&)> snapshot,std::function<void(net::Packet)> send) {
    if(!snapshot || !send)throw std::invalid_argument("Missing guild game-log service");
    return [snapshot=std::move(snapshot),send=std::move(send)](GuildUser& user,const LoadedGuild& guild,std::uint16_t type,std::uint8_t level) {
        send(guild_npc_log_packet(snapshot(user),guild,type,level));
    };
}
}
