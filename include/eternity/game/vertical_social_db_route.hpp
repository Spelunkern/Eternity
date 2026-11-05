#pragma once
#include "eternity/game/vertical_social_lifecycle.hpp"

namespace eternity::game {
// ps_dbAgent.exe CUser::PacketUserFriend 00406930: 0c02/03/04/06/07/08
// packets identify the retained user by account at offset 2. The lookup must
// retain the matching lifecycle through apply and share its save mutex.
template<class Database>
EntryCatalogUpdate vertical_social_db_route(std::span<const std::uint8_t> packet,
    const std::function<VerticalSocialLifecycle<Database>*(std::uint32_t)>& find_owner) {
    if(packet.size()<2)return EntryCatalogUpdate::rejected;
    if(packet[1]!=0x0c || (packet[0]!=2 && packet[0]!=3 && packet[0]!=4 && packet[0]!=6 && packet[0]!=7 && packet[0]!=8))
        return EntryCatalogUpdate::unsupported;
    if(packet.size()<6)return EntryCatalogUpdate::rejected;
    if(!find_owner)throw std::invalid_argument("Missing social DB owner lookup");
    std::uint32_t account=0;for(unsigned i=0;i<4;++i)account|=std::uint32_t{packet[2+i]}<<(8*i);
    auto* owner=find_owner(account);if(!owner)return EntryCatalogUpdate::rejected;
    return owner->apply(packet);
}
}
