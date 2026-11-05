#pragma once
#include "eternity/game/entry_catalogs.hpp"
#include <algorithm>
#include <optional>

namespace eternity::game {
// CUser::PacketComm 00475870, 2202 branch 004758ad..00475ac9.
// Fields and catalog are retained by the real serialized live-user owner.
// Each live user must have one stable wrapper so pointer equality detects self.
struct SocialFriendUser {
    SocialCatalog& catalog;
    std::uint32_t list_id, wire_id; // Character ID CUser+128; entity ID CUser+dc.
    std::uint8_t country, family, grow, job;
    std::string name;
    std::uint32_t& pending_requester;
    std::uint32_t& pending_deadline;
};
struct SocialFriendRequestPorts {
    // Resolve via CWorld::FindUserZoneMove(name), then the caller's CZone::FindUser(id).
    std::function<SocialFriendUser*(std::string_view)> find_in_zone;
    std::function<std::uint32_t()> tick;
    std::function<void(net::Packet)> send_db;
    std::function<void(SocialFriendUser&,net::Packet)> send;
};
inline EntryCatalogUpdate social_friend_request(SocialFriendUser& owner,std::span<const std::uint8_t> p,const SocialFriendRequestPorts& ports) {
    if(p.size()<2)return EntryCatalogUpdate::rejected;
    if(p[0]!=2 || p[1]!=0x22)return EntryCatalogUpdate::unsupported;
    if(!ports.find_in_zone || !ports.tick || !ports.send_db || !ports.send)throw std::invalid_argument("Missing friend request services");
    if(p.size()<23)return EntryCatalogUpdate::rejected;
    auto end=std::find(p.begin()+2,p.begin()+22,0);std::string name(p.begin()+2,end);
    const auto contains_name=[](const SocialCatalog& c,std::string_view n){return std::any_of(c.friends().begin(),c.friends().end(),[&](const auto& row){return row.name==n;});};
    SocialFriendUser* target=nullptr;
    if(!contains_name(owner.catalog,name))target=ports.find_in_zone(name);
    if(!target || owner.catalog.find(target->list_id) || target==&owner || target->country!=owner.country || target->pending_requester) {
        ports.send(owner,{3,0x22,2});return EntryCatalogUpdate::applied;
    }
    if(owner.name.size()>20 || target->name.size()>20 || owner.name.find('\0')!=std::string::npos || target->name.find('\0')!=std::string::npos)
        throw std::invalid_argument("Invalid live character name");
    const auto append=[](net::Packet& packet,std::uint32_t v){for(unsigned i=0;i<4;++i)packet.push_back(static_cast<std::uint8_t>(v>>(8*i)));};
    if(!contains_name(target->catalog,owner.name)) {
        target->pending_requester=owner.wire_id;target->pending_deadline=ports.tick()+30000U;
        net::Packet response{2,0x22};response.insert(response.end(),owner.name.begin(),owner.name.end());response.resize(23,0);ports.send(*target,std::move(response));
    } else {
        owner.catalog.add_friend({target->list_id,target->family,target->grow,target->job,target->name,{}});
        net::Packet db{2,0x0c};append(db,owner.catalog.account_id());append(db,target->wire_id);db.insert(db.end(),{target->family,target->grow,target->job});db.insert(db.end(),target->name.begin(),target->name.end());db.resize(34,0);ports.send_db(std::move(db));
        ports.send(owner,{3,0x22,1});
        net::Packet added{4,0x22};append(added,target->wire_id);added.push_back(target->job);added.insert(added.end(),target->name.begin(),target->name.end());added.resize(28,0);ports.send(owner,std::move(added));
        net::Packet online{7,0x22};append(online,owner.wire_id);online.push_back(1);ports.send(*target,std::move(online));
    }
    return EntryCatalogUpdate::applied;
}
// CUser::PacketComm 00475ace..00475cff: resolve in world, clear the
// pending fields, then resolve the returned user in the caller's zone.
struct SocialFriendResponsePorts {
    std::function<std::optional<std::uint32_t>(std::uint32_t)> find_world_id;
    std::function<SocialFriendUser*(std::uint32_t)> find_zone_id;
    std::function<void(net::Packet)> send_db;
    std::function<void(SocialFriendUser&,net::Packet)> send;
};
inline EntryCatalogUpdate social_friend_response(SocialFriendUser& owner,std::span<const std::uint8_t> p,const SocialFriendResponsePorts& ports) {
    if(p.size()<2)return EntryCatalogUpdate::rejected;
    if(p[0]!=3 || p[1]!=0x22)return EntryCatalogUpdate::unsupported;
    if(p.size()<3)return EntryCatalogUpdate::rejected;
    if(!ports.find_world_id || !ports.find_zone_id || !ports.send_db || !ports.send)throw std::invalid_argument("Missing friend response services");
    if(!owner.pending_requester)return EntryCatalogUpdate::applied;
    auto world_id=ports.find_world_id(owner.pending_requester);
    owner.pending_requester=0;owner.pending_deadline=0;
    if(!world_id)return EntryCatalogUpdate::applied;
    auto* peer=ports.find_zone_id(*world_id);
    if(!peer)return EntryCatalogUpdate::applied;
    if(owner.catalog.find(peer->list_id) || p[2]!=1) {ports.send(*peer,{3,0x22,0});return EntryCatalogUpdate::applied;}
    if(owner.name.size()>20 || peer->name.size()>20 || owner.name.find('\0')!=std::string::npos || peer->name.find('\0')!=std::string::npos)
        throw std::invalid_argument("Invalid live character name");
    owner.catalog.add_friend({peer->list_id,peer->family,peer->grow,peer->job,peer->name,{}});
    peer->catalog.add_friend({owner.list_id,owner.family,owner.grow,owner.job,owner.name,{}});
    const auto append=[](net::Packet& out,std::uint32_t v){for(unsigned i=0;i<4;++i)out.push_back(static_cast<std::uint8_t>(v>>(8*i)));};
    const auto db_packet=[&](std::uint32_t account,const SocialFriendUser& value,std::uint8_t family,std::uint8_t grow){
        net::Packet out{2,0x0c};append(out,account);append(out,value.wire_id);out.insert(out.end(),{family,grow,value.job});out.insert(out.end(),value.name.begin(),value.name.end());out.resize(34,0);return out;
    };
    ports.send_db(db_packet(owner.catalog.account_id(),*peer,peer->family,peer->grow));
    // Original reuses the first packet's family/grow bytes for the second DB send.
    ports.send_db(db_packet(peer->catalog.account_id(),owner,peer->family,peer->grow));
    ports.send(*peer,{3,0x22,1});
    const auto added=[&](const SocialFriendUser& value){net::Packet out{4,0x22};append(out,value.wire_id);out.push_back(value.job);out.insert(out.end(),value.name.begin(),value.name.end());out.resize(28,0);return out;};
    ports.send(*peer,added(owner));ports.send(owner,added(*peer));
    return EntryCatalogUpdate::applied;
}

// ps_game.exe CUser::UpdateTimeover 0048e310, friend branch 0048e3e0.
// The original uses an unsigned absolute comparison, including at tick wrap.
inline bool social_friend_timeout(SocialFriendUser& owner,std::uint32_t now) {
    if(!owner.pending_deadline || now<owner.pending_deadline || !owner.pending_requester)return false;
    owner.pending_deadline=0;owner.pending_requester=0;return true;
}

}
