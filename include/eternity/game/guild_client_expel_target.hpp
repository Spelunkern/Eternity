#pragma once
#include "eternity/game/guild_catalog.hpp"
#include "eternity/game/zone_leave_guild_creation.hpp"

namespace eternity::game {
// CGuild::JoinExpel 0042dcb0..0042dd90. Existing lower-privilege records
// are left to the full online/offline removal owner.
inline EntryCatalogUpdate guild_client_expel_target(GuildUser& user,Actor& actor,GuildCatalog& catalog,
    ZoneGuildCreationServices& creation,std::span<const std::uint8_t> packet,const std::function<void(GuildUser&,net::Packet)>& send) {
    if(packet.size()<2)return EntryCatalogUpdate::rejected;
    if(packet[0]!=10 || packet[1]!=13)return EntryCatalogUpdate::unsupported;
    if(packet.size()<6)return EntryCatalogUpdate::rejected;
    auto* captured=user.guild;if(!captured)return EntryCatalogUpdate::applied;
    auto* guild=catalog.find_membership(captured);if(!guild)throw std::logic_error("Foreign captured guild owner");
    std::lock_guard guard(guild->mutex);
    if(user.rank>2 || !user.guild || creation.current(actor))return EntryCatalogUpdate::applied;
    std::uint32_t id=0;for(unsigned i=0;i<4;++i)id|=std::uint32_t{packet[2+i]}<<(i*8);
    GuildMembership::Record record;const auto online=guild->membership.online.find(id);
    if(online!=guild->membership.online.end())record=online->second;
    if(!record) {const auto offline=guild->membership.offline.find(id);if(offline!=guild->membership.offline.end())record=offline->second;}
    if(!record)return EntryCatalogUpdate::applied;
    if(record->rank>user.rank)return EntryCatalogUpdate::unsupported;
    if(!send)throw std::invalid_argument("Missing guild expulsion client transport");
    net::Packet response{10,13,0};for(unsigned i=0;i<4;++i)response.push_back(static_cast<std::uint8_t>(id>>(8*i)));
    send(user,std::move(response));return EntryCatalogUpdate::applied;
}
}
