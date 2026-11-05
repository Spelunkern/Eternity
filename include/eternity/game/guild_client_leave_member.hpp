#pragma once
#include "eternity/game/guild_catalog.hpp"
#include "eternity/game/guild_leave_duel.hpp"
#include "eternity/game/zone_leave_guild_creation.hpp"

namespace eternity::game {
struct GuildLeaveMemberPorts {
    std::function<void(GuildUser&,net::Packet)> send_user;
    std::function<void(net::Packet)> send_db;
    std::function<void(GuildUser&,Actor&,const LoadedGuild&)> game_log;
    std::function<void(Actor&,LoadedGuild&)> delete_empty;
};
// CUser::LeaveGuild 0042da90 and PacketGuild 00476fbc. The caller owns the
// world executor and retains all services. Last-member requests require the
// actual empty-guild manager owner before any membership mutation.
inline EntryCatalogUpdate guild_client_leave_member(GuildUser& user,Actor& actor,GuildCatalog& catalog,
    ZoneGuildCreationServices& creation,ZoneLeaveGuildDuelServices& duel,std::span<const std::uint8_t> packet,
    const GuildLeaveMemberPorts& ports) {
    if(packet.size()<2)return EntryCatalogUpdate::rejected;
    if(packet[0]!=9 || packet[1]!=13)return EntryCatalogUpdate::unsupported;
    if(user.character_id!=actor.id())throw std::logic_error("Guild leave actor mismatch");
    auto* captured=user.guild;if(!captured)return EntryCatalogUpdate::applied;
    auto* guild=catalog.find_membership(captured);if(!guild)throw std::logic_error("Foreign captured guild owner");
    std::lock_guard guard(guild->mutex);
    if(!user.guild || creation.current(actor))return EntryCatalogUpdate::applied;
    auto& membership=guild->membership;const auto count=static_cast<std::uint32_t>(membership.online.size())+static_cast<std::uint32_t>(membership.offline.size());
    if((user.rank<=1 && count!=1) || count==0 || (count==1 && !ports.delete_empty))return EntryCatalogUpdate::unsupported;
    auto found=membership.online.find(user.character_id);if(found==membership.online.end() || !found->second)return EntryCatalogUpdate::unsupported;
    if(!ports.send_user || !ports.send_db || !ports.game_log)throw std::invalid_argument("Missing guild leave services");
    auto node=membership.online.extract(found);auto record=std::move(node.mapped());
    if(user.rank<=2) {--guild->metadata.officers;for(auto& leader:membership.leaders)if(leader==record){leader.reset();break;}}
    catalog.directory().remove(record->id);user.guild_id=0;user.rank=0;
    guild_leave_duel(actor,duel);user.guild=nullptr;
    auto append=[](net::Packet& p,std::uint32_t value){for(unsigned i=0;i<4;++i)p.push_back(static_cast<std::uint8_t>(value>>(i*8)));};
    net::Packet db{8,11};append(db,guild->metadata.listing.id);append(db,user.character_id);ports.send_db(std::move(db));
    ports.send_user(user,{9,13,1});net::Packet notice{12,13,0x66};append(notice,user.character_id);membership.broadcast(std::move(notice));
    ports.game_log(user,actor,*guild);
    const auto remaining=static_cast<std::uint32_t>(membership.online.size())+static_cast<std::uint32_t>(membership.offline.size());
    if(!remaining) {
        if(!ports.delete_empty)throw std::logic_error("Guild became empty without manager service");
        ports.delete_empty(actor,*guild);
    }
    return EntryCatalogUpdate::applied;
}
}
