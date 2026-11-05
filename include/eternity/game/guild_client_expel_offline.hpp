#pragma once
#include "eternity/game/guild_catalog.hpp"
#include "eternity/game/zone_leave_guild_creation.hpp"

namespace eternity::game {
struct GuildExpelOfflinePorts {
    std::function<void(GuildUser&,net::Packet)> send_user;
    std::function<void(net::Packet)> send_db;
    std::function<void(Actor&,const GuildMember&,GuildUser*,const LoadedGuild&)> game_log;
};
// CGuild::JoinExpel 0042dd90..0042e021 for records without a live CUser.
// The catalog, requester, guild and services outlive all synchronous callbacks.
inline EntryCatalogUpdate guild_client_expel_offline(GuildUser& user,Actor& actor,GuildCatalog& catalog,
    ZoneGuildCreationServices& creation,std::span<const std::uint8_t> packet,const GuildExpelOfflinePorts& ports) {
    if(packet.size()<2)return EntryCatalogUpdate::rejected;
    if(packet[0]!=10 || packet[1]!=13)return EntryCatalogUpdate::unsupported;
    if(packet.size()<6)return EntryCatalogUpdate::rejected;
    if(user.character_id!=actor.id())throw std::logic_error("Guild expulsion actor mismatch");
    auto* captured=user.guild;if(!captured)return EntryCatalogUpdate::applied;
    auto* guild=catalog.find_membership(captured);if(!guild)throw std::logic_error("Foreign captured guild owner");
    std::lock_guard guard(guild->mutex);if(user.rank>2 || !user.guild || creation.current(actor))return EntryCatalogUpdate::applied;
    std::uint32_t id=0;for(unsigned i=0;i<4;++i)id|=std::uint32_t{packet[2+i]}<<(8*i);
    auto& membership=guild->membership;auto online=membership.online.find(id);GuildMembership::Record record;
    if(online!=membership.online.end())record=online->second;
    if(!record){auto offline=membership.offline.find(id);if(offline!=membership.offline.end())record=offline->second;}
    if(!record)return EntryCatalogUpdate::applied;
    if(record->rank<=user.rank || record->user)return EntryCatalogUpdate::unsupported;
    if(!ports.send_user || !ports.send_db || !ports.game_log)throw std::invalid_argument("Missing guild expulsion services");
    auto node=membership.online.extract(id);record=node.empty()?nullptr:std::move(node.mapped());
    if(!record){node=membership.offline.extract(id);record=node.empty()?nullptr:std::move(node.mapped());}
    if(!record)throw std::logic_error("Guild expulsion record disappeared");
    catalog.directory().remove(record->id);if(record->rank<=2)--guild->metadata.officers;
    auto append=[](net::Packet& p,std::uint32_t value){for(unsigned i=0;i<4;++i)p.push_back(static_cast<std::uint8_t>(value>>(8*i)));};
    net::Packet db{8,11};append(db,guild->metadata.listing.id);append(db,record->id);ports.send_db(std::move(db));
    ports.game_log(actor,*record,nullptr,*guild);net::Packet response{10,13,1};append(response,id);ports.send_user(user,std::move(response));
    net::Packet notice{12,13,0x67};append(notice,id);membership.broadcast(std::move(notice));return EntryCatalogUpdate::applied;
}
}
