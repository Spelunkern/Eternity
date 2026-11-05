#pragma once
#include "eternity/game/social_block_add.hpp"
#include "eternity/game/social_friend_request.hpp"

namespace eternity::game {
// Serialized CUser::PacketComm 00475870 social subset. These ports must bind
// the live world/zone and real DB/client transports; each handled operation
// validates its required services. The DB transport routes by packet account.
struct VerticalSocialClientPorts {
    std::function<SocialFriendUser*(std::string_view)> friend_by_name_in_zone;
    std::function<std::optional<std::uint32_t>(std::uint32_t)> friend_world_id;
    std::function<SocialFriendUser*(std::uint32_t)> friend_by_id_in_zone;
    SocialBlockLookup block_by_name;
    std::function<std::uint32_t()> tick;
    SocialCatalog::Lookup catalog_by_id;
    std::function<void(net::Packet)> send_db;
    std::function<void(SocialFriendUser&,net::Packet)> send_user;
    std::function<void(std::uint32_t,net::Packet)> send_peer;
    // Real party/zone owner, including its ordered locked traversal.
    std::function<EntryCatalogUpdate(std::span<const std::uint8_t>)> party_find;
};
inline EntryCatalogUpdate vertical_social_client(SocialFriendUser& owner,std::span<const std::uint8_t> packet,const VerticalSocialClientPorts& ports) {
    if(packet.size()<2)return EntryCatalogUpdate::rejected;
    if(packet[1]!=0x22)return EntryCatalogUpdate::unsupported;
    switch(packet[0]) {
    case 2:
        return social_friend_request(owner,packet,{ports.friend_by_name_in_zone,ports.tick,ports.send_db,ports.send_user});
    case 3:
        return social_friend_response(owner,packet,{ports.friend_world_id,ports.friend_by_id_in_zone,ports.send_db,ports.send_user});
    case 12:
        if(!ports.party_find)throw std::invalid_argument("Missing party-find owner");
        return ports.party_find(packet);
    case 9:
        if(!ports.send_user)throw std::invalid_argument("Missing social client transport");
        return social_block_add(owner.catalog,owner.country,packet,ports.block_by_name,ports.send_db,
            [&](net::Packet p){ports.send_user(owner,std::move(p));});
    case 5:case 6:case 10:case 11:
        if(!ports.send_user)throw std::invalid_argument("Missing social client transport");
        return owner.catalog.apply_client(packet,ports.send_db,
            [&](net::Packet p){ports.send_user(owner,std::move(p));},ports.catalog_by_id,ports.send_peer);
    default:return EntryCatalogUpdate::unsupported;
    }
}
}
