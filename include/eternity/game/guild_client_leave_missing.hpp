#pragma once
#include "eternity/game/guild_catalog.hpp"
#include "eternity/game/zone_leave_guild_creation.hpp"

namespace eternity::game {
// CUser::LeaveGuild 0042dace..0042daec and failure 0042dc75.
// A real online record is left untouched for the full removal owner.
inline EntryCatalogUpdate guild_client_leave_missing(GuildUser& user,Actor& actor,GuildCatalog& catalog,
    ZoneGuildCreationServices& creation,std::span<const std::uint8_t> packet,const std::function<void(GuildUser&,net::Packet)>& send) {
    if(packet.size()<2)return EntryCatalogUpdate::rejected;
    if(packet[0]!=9 || packet[1]!=13)return EntryCatalogUpdate::unsupported;
    auto* captured=user.guild;if(!captured)return EntryCatalogUpdate::applied;
    auto* guild=catalog.find_membership(captured);if(!guild)throw std::logic_error("Foreign captured guild owner");
    std::lock_guard guard(guild->mutex);
    if(!user.guild || creation.current(actor))return EntryCatalogUpdate::applied;
    const auto count=static_cast<std::uint32_t>(guild->membership.online.size())+static_cast<std::uint32_t>(guild->membership.offline.size());
    const bool leader_blocked=user.rank<=1 && count!=1;
    auto row=guild->membership.online.find(user.character_id);
    if(!leader_blocked && row!=guild->membership.online.end() && row->second)return EntryCatalogUpdate::unsupported;
    if(!send)throw std::invalid_argument("Missing guild leave client transport");
    if(!leader_blocked && row!=guild->membership.online.end())guild->membership.online.erase(row);
    send(user,net::Packet{9,13,0});return EntryCatalogUpdate::applied;
}
}
