#include "eternity/game/vertical_inventory.hpp"
#include "eternity/game/actor.hpp"

namespace eternity::game {
VerticalInventoryResult vertical_inventory_request(Actor& actor,std::span<const std::uint8_t> packet,
    ZoneItems& zone,const ItemQuestLinks& links,ItemPickupWorld& world,GroundItemPool& pool,
    VerticalInventoryActions& actions) {
    if(packet.size()<2) return {VerticalInventoryRequest::malformed,{}};
    if(packet[0]!=0x05 || packet[1]!=0x02) return {VerticalInventoryRequest::unhandled,{}};
    if(packet.size()<6) return {VerticalInventoryRequest::malformed,{}};
    if(actor.attack_state().mode==1) return {VerticalInventoryRequest::ignored,{}};
    actions.cancel_action_exchange(actor);
    actions.end_shop(actor);
    // Original loads the ID after both callbacks; nested code can alter the buffer.
    std::uint32_t id=0;
    for(unsigned i=0;i<4;++i) id|=std::uint32_t(packet[2+i])<<(8*i);
    return {VerticalInventoryRequest::pickup,actor.pickup_item(zone,id,actions.pickup_checks(),links,world,pool)};
}
}
