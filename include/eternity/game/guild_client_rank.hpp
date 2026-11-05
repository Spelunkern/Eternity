#pragma once
#include "eternity/game/guild_catalog.hpp"

namespace eternity::game {
// ps_game.exe CUser::PacketGuild 00477057 / CGuild::ChangeGuildUserLevel 0042e9f0.
// Entity and character IDs alias at this admitted-session stage.
inline EntryCatalogUpdate guild_client_rank(GuildUser& user,GuildCatalog& catalog,std::span<const std::uint8_t> packet,
    const std::function<void(net::Packet)>& send_db) {
    if(packet.size()<2)return EntryCatalogUpdate::rejected;
    if(packet[0]!=0x0c || packet[1]!=0x0d)return EntryCatalogUpdate::unsupported;
    if(packet.size()<7)return EntryCatalogUpdate::rejected;
    std::uint32_t id=0;for(unsigned i=0;i<4;++i)id|=std::uint32_t{packet[3+i]}<<(8*i);
    auto* captured=user.guild;
    if(!captured)return EntryCatalogUpdate::applied;
    auto* locked=catalog.find_membership(captured);if(!locked)throw std::logic_error("Foreign captured guild owner");
    std::lock_guard guard(locked->mutex);
    if(id==user.character_id || (user.rank!=1 && user.rank!=2) || !user.guild)return EntryCatalogUpdate::applied;
    auto* guild=catalog.find_membership(user.guild);if(!guild)throw std::logic_error("Foreign current guild owner");
    const int delta=packet[2]==0?-1:packet[2]==1?1:0;if(!delta)return EntryCatalogUpdate::applied;
    auto& membership=guild->membership;auto find=[&](const GuildMembership::Members& rows)->GuildMembership::Record {auto it=rows.find(id);return it==rows.end()?nullptr:it->second;};
    auto member=find(membership.online);if(!member)member=find(membership.offline);if(!member)return EntryCatalogUpdate::applied;
    const auto old=member->rank;const auto next=old+delta;
    if(old<=user.rank || next<2 || next>9 || (next==2 && guild->metadata.officers>=7) || (user.rank==2 && next==2))return EntryCatalogUpdate::applied;
    if(!send_db)throw std::invalid_argument("Missing guild rank DB transport");
    if(delta==1 && old==2) {--guild->metadata.officers;if(member->user)for(auto& leader:membership.leaders)if(leader==member){leader.reset();break;}}
    if(delta==-1 && next==2) {++guild->metadata.officers;if(member->user)for(auto& leader:membership.leaders)if(!leader){leader=member;break;}}
    member->rank=next;if(member->user)member->user->rank=next;
    net::Packet db{9,11};const auto append=[](net::Packet& p,std::uint32_t v){for(unsigned i=0;i<4;++i)p.push_back(static_cast<std::uint8_t>(v>>(8*i)));};
    append(db,guild->metadata.listing.id);append(db,member->id);db.push_back(static_cast<std::uint8_t>(next));send_db(std::move(db));
    net::Packet response{0x0c,0x0d,static_cast<std::uint8_t>(next-0x38)};append(response,id);membership.broadcast(std::move(response));
    return EntryCatalogUpdate::applied;
}
}
