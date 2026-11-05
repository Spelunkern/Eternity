#pragma once
#include "eternity/game/guild_catalog.hpp"
#include <memory>

namespace eternity::game {
struct GuildApplicationAcceptPorts {
    std::function<std::shared_ptr<GuildUser>(std::uint32_t)> find_online;
    std::function<std::uint16_t(GuildUser&)> online_level;
    std::function<void(GuildUser&,net::Packet)> send_user;
    std::function<void(net::Packet)> send_db;
    // Must use the retained GuildBank owner; an absent bank is not an empty bank.
    std::function<void(GuildUser&,const LoadedGuild&)> send_bank;
    std::function<std::uint32_t()> tick;
    std::function<void(GuildUser&,std::uint32_t)> set_join_deadline;
    // The real game-log owner handles online and offline 0b04 records.
    std::function<void(const GuildMember&,GuildUser*,const LoadedGuild&)> game_log;
};
// ps_game.exe PacketGuild 00476f50 action 1 and CGuild::JoinOK 0042e330.
// Caller retains requester, catalog, services and target session through callbacks.
inline EntryCatalogUpdate guild_client_accept(GuildUser& requester,GuildCatalog& catalog,
    std::span<const std::uint8_t> packet,const GuildApplicationAcceptPorts& ports) {
    if(packet.size()<2)return EntryCatalogUpdate::rejected;
    if(packet[0]!=8 || packet[1]!=13)return EntryCatalogUpdate::unsupported;
    if(packet.size()<7)return EntryCatalogUpdate::rejected;
    auto* captured=requester.guild;if(!captured)return EntryCatalogUpdate::applied;
    auto* locked=catalog.find_membership(captured);if(!locked)throw std::logic_error("Foreign captured guild owner");
    std::lock_guard guard(locked->mutex);
    if(requester.rank>2 || !requester.guild)return EntryCatalogUpdate::applied;
    auto* guild=catalog.find_membership(requester.guild);if(!guild)throw std::logic_error("Foreign current guild owner");
    if(packet[2]!=1)return EntryCatalogUpdate::unsupported;
    std::uint32_t character=0;for(unsigned i=0;i<4;++i)character|=std::uint32_t{packet[3+i]}<<(8*i);
    auto& membership=guild->membership;auto it=membership.applications.find(character);
    if(it==membership.applications.end())return EntryCatalogUpdate::applied;
    if(!it->second){membership.applications.erase(it);return EntryCatalogUpdate::applied;}
    if(!ports.find_online || !ports.online_level || !ports.send_user || !ports.send_db || !ports.send_bank || !ports.tick || !ports.set_join_deadline || !ports.game_log)
        throw std::invalid_argument("Missing guild acceptance services");
    auto record=it->second;membership.applications.erase(it);record->rank=9;
    auto peer=ports.find_online(character);
    if(peer) {
        record->level=ports.online_level(*peer);record->user=peer.get();peer->rank=9;peer->guild_id=guild->metadata.listing.id;peer->request_id=0;peer->guild=&membership;
        membership.online.emplace(record->id,record);
    } else {record->user=nullptr;membership.offline.emplace(record->id,record);}
    auto append=[](net::Packet& p,std::uint32_t value,unsigned width=4){for(unsigned i=0;i<width;++i)p.push_back(static_cast<std::uint8_t>(value>>(8*i)));};
    net::Packet db{9,11};append(db,guild->metadata.listing.id);append(db,record->id);db.push_back(static_cast<std::uint8_t>(record->rank));ports.send_db(std::move(db));
    net::Packet admin{0x18,13};append(admin,record->id);membership.broadcast_admin(std::move(admin));
    const auto& name=record->name;if(name.size()>20 || name.find('\0')!=name.npos)throw std::invalid_argument("Invalid guild applicant name");
    net::Packet notice{0x14,13,static_cast<std::uint8_t>(peer?1:0)};append(notice,record->id);notice.push_back(static_cast<std::uint8_t>(record->rank));append(notice,static_cast<std::uint32_t>(record->level),2);notice.push_back(static_cast<std::uint8_t>(record->job));notice.insert(notice.end(),name.begin(),name.end());notice.resize(32,0);membership.broadcast(std::move(notice));
    if(peer) {
        const auto& guild_name=guild->metadata.listing.name;if(guild_name.size()>24 || guild_name.find('\0')!=guild_name.npos)throw std::invalid_argument("Invalid guild name");
        net::Packet response{8,13,1};append(response,guild->metadata.listing.id);response.push_back(9);response.insert(response.end(),guild_name.begin(),guild_name.end());response.resize(33,0);ports.send_user(*peer,std::move(response));
        membership.send_users(*peer);ports.send_bank(*peer,*guild);
        if(auto npc=guild->npcs.client_packet())ports.send_user(*peer,std::move(*npc));
        ports.set_join_deadline(*peer,ports.tick()+0x0f731400u);
    }
    ports.game_log(*record,peer.get(),*guild);return EntryCatalogUpdate::applied;
}
}
