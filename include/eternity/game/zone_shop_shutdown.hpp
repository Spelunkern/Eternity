#pragma once
#include "eternity/game/zone_leave_interactions.hpp"

namespace eternity::game {
class ZoneShopShutdownServices {
public:
    virtual ~ZoneShopShutdownServices()=default;
    // Lookup uses the owner's still-retained zone and the shop's live customer IDs.
    virtual Actor* find_customer(Actor&,std::uint32_t)=0;
    virtual std::shared_ptr<void> retain(Actor&)=0;
    virtual ZoneLeaveShopState& shop_state(Actor&)=0;
    // Null when CUser+e0 is null, otherwise the current original zone cell.
    virtual std::optional<CellCoordinates> cell(Actor&)=0;
    virtual void send_view(CellCoordinates,const net::Packet&)=0;
};
// ps_game.exe CUserShop::Ended 00478520 and customer loop 00478720.
// The world serializes shop/zone state, retains the owner, and keeps each
// returned peer and scene alive for its callback; callbacks must not throw.
inline void zone_shutdown_shop(Actor& owner,ZoneLeaveShopState& shop,ZoneShopShutdownServices& services) {
    if(shop.mode!=1 && shop.mode!=2)return;
    if(shop.customer_count<0 || static_cast<std::size_t>(shop.customer_count)>shop.customer_ids.size())
        throw std::logic_error("Invalid shop customer count");
    shop.mode=0;
    // Live count/slots are sampled each iteration, as in the original loop.
    for(std::int32_t i=0;i<shop.customer_count;++i) {
        if(static_cast<std::size_t>(i)>=shop.customer_ids.size())throw std::logic_error("Shop customer count changed out of bounds");
        if(auto* customer=services.find_customer(owner,shop.customer_ids[i])) {
            auto lease=services.retain(*customer);if(!lease)throw std::logic_error("Cannot retain shop customer");
            services.shop_state(*customer).visited_owner=0;
            customer->send_self({0x0c,0x23});
        }
    }
    shop.offer_bytes.fill(0);shop.customer_count=0;owner.send_self({2,0x23});
    if(auto cell=services.cell(owner)) {
        net::Packet packet{0x23,5};for(unsigned i=0;i<4;++i)packet.push_back(static_cast<std::uint8_t>(owner.id()>>(8*i)));
        packet.push_back(0);services.send_view(*cell,packet);
    }
}
}
