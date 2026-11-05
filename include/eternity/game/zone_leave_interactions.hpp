#pragma once
#include "eternity/game/vertical_move_interactions.hpp"
#include "eternity/game/zone_leave_exchange.hpp"
#include <array>

namespace eternity::game {
// CUser embedded shop state: customer slots +16f8, count +16f4 and currently
// visited shop-owner ID +1738. Inactive slots retain their original contents.
struct ZoneLeaveShopState {
    std::int32_t mode=0; // CUserShop+4; only 1 and 2 are active.
    std::array<std::uint8_t,48> offer_bytes{}; // CUserShop+8..+37.
    std::uint32_t visited_owner=0;
    std::int32_t customer_count=0;
    std::array<std::uint32_t,16> customer_ids{};
};
class ZoneLeaveInteractionServices {
public:
    virtual ~ZoneLeaveInteractionServices()=default;
    // CZone::FindUser 00415f30, using the subject's still-bound zone.
    virtual Actor* find_in_zone(Actor&,std::uint32_t)=0;
    // Full own CUserShop shutdown 00478520 including notifications and effects.
    virtual void shutdown_shop(Actor&,ZoneLeaveShopState&)=0;
    virtual std::shared_ptr<void> retain(Actor&)=0;
    virtual std::uint32_t& exchange_timer(Actor&)=0;
};
// Prefix of CUser::LeaveZone 0044aee0..0044b04c. The actor, zone and NPC
// survive calls and the world serializes the operation. The remaining vehicle,
// invitations, party and duel branches must run after this prefix.
inline void zone_leave_interactions(Actor& actor,VerticalNpcInteraction& npc,
    ZoneLeaveShopState& shop,ZoneLeaveInteractionServices& services) {
    if(shop.customer_count<0 || static_cast<std::size_t>(shop.customer_count)>shop.customer_ids.size())
        throw std::logic_error("Invalid retained shop customer count");
    if(auto* current=npc.npc) {
        current->users_using=std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(current->users_using)-1U);
        npc.npc=nullptr;
    }
    if(shop.visited_owner && services.find_in_zone(actor,shop.visited_owner)) {
        shop.visited_owner=0;
        for(std::int32_t i=0;i<shop.customer_count;++i)if(shop.customer_ids[i]==actor.id()) {
            shop.customer_ids[i]=shop.customer_ids[shop.customer_count-1];--shop.customer_count;break;
        }
    }
    services.shutdown_shop(actor,shop);
    zone_leave_exchange(actor,[&](Actor& peer){return services.retain(peer);},
        [&](Actor& subject)->std::uint32_t&{return services.exchange_timer(subject);});
}
}
