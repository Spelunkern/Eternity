#pragma once
#include "eternity/game/guild_catalog.hpp"
#include "eternity/game/guild_leave_duel.hpp"
#include "eternity/game/zone_leave_guild_creation.hpp"

namespace eternity::game {
struct GuildExpelOnlinePorts {
    std::function<std::shared_ptr<Actor>(GuildUser&)> retain_actor;
    std::function<ZoneLeaveGuildDuelServices&(GuildUser&)> duel_for;
    std::function<void(GuildUser&,net::Packet)> send_user,send_target;
    std::function<void(net::Packet)> send_db;
    std::function<void(Actor&,const GuildMember&,GuildUser*,const LoadedGuild&)> game_log;
};
// CGuild::JoinExpel 0042dd90..0042dee1 for a record with a live CUser.
// The caller serializes the world and retains the requester and catalog.
inline EntryCatalogUpdate guild_client_expel_online(GuildUser& user,Actor& actor,GuildCatalog& catalog,
    ZoneGuildCreationServices& creation,std::span<const std::uint8_t> packet,const GuildExpelOnlinePorts& ports) {
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
    if(record->rank<=user.rank || !record->user)return EntryCatalogUpdate::unsupported;
    if(!ports.retain_actor || !ports.duel_for || !ports.send_user || !ports.send_target || !ports.send_db || !ports.game_log)
        throw std::invalid_argument("Missing online guild expulsion services");
    auto* peer=record->user;if(peer->character_id!=record->id)throw std::logic_error("Expelled member identity mismatch");
    auto target=ports.retain_actor(*peer);if(!target || target->id()!=record->id)throw std::logic_error("Expelled actor unavailable");
    auto& duel=ports.duel_for(*peer);
    auto current=membership.online.find(id);GuildMembership::Record checked;
    if(current!=membership.online.end())checked=current->second;
    if(!checked){auto offline=membership.offline.find(id);if(offline!=membership.offline.end())checked=offline->second;}
    if(user.guild!=captured || checked!=record || record->user!=peer || record->rank<=user.rank)
        throw std::logic_error("Guild expulsion changed during owner lookup");
    auto node=membership.online.extract(id);record=node.empty()?nullptr:std::move(node.mapped());
    if(!record){node=membership.offline.extract(id);record=node.empty()?nullptr:std::move(node.mapped());}
    if(!record || record->user!=peer)throw std::logic_error("Guild expulsion record changed");
    catalog.directory().remove(record->id);
    if(record->rank<=2){--guild->metadata.officers;for(auto& leader:membership.leaders)if(leader==record){leader.reset();break;}}
    auto append=[](net::Packet& p,std::uint32_t value){for(unsigned i=0;i<4;++i)p.push_back(static_cast<std::uint8_t>(value>>(8*i)));};
    net::Packet response{10,13,1};append(response,id);ports.send_target(*peer,response);
    peer->guild_id=0;peer->rank=0;guild_leave_duel(*target,duel);peer->guild=nullptr;
    net::Packet db{8,11};append(db,guild->metadata.listing.id);append(db,record->id);ports.send_db(std::move(db));
    ports.game_log(actor,*record,peer,*guild);ports.send_user(user,std::move(response));
    net::Packet notice{12,13,0x67};append(notice,id);membership.broadcast(std::move(notice));return EntryCatalogUpdate::applied;
}
}
