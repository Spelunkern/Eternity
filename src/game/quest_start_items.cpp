#include "eternity/game/quest_start_items.hpp"
#include "eternity/game/item_pool.hpp"
namespace eternity::game {
bool quest_start_items_add(std::int32_t type,std::int32_t id,std::int32_t count,
    QuestStartItemsResult& result,QuestStartItemsUser& user,QuestStartItemsWorld& world) {
    const auto* definition=world.find_definition(type,id);result.definition=definition;
    if(!definition) {result.bag=0;result.slot=0;return false;}
    const auto bags=user.open_bags;
    for(std::int32_t bag=1;bag<=bags;++bag) for(std::int32_t slot=0;slot<24;++slot) {
        if(!world.slot_empty(bag,slot)) continue;
        auto* item=world.create_item();if(!item) {result.bag=0;result.slot=0;return false;}
        item->definition=definition;
        const auto made=world.local_make_time();
        initialize_ground_item(*item,definition->type,definition->type_id,static_cast<std::uint8_t>(count),made,'Q');
        item->item.id=definition->id;item->item.uid=world.next_uid();item->item.bag=static_cast<std::uint8_t>(bag);item->item.slot=static_cast<std::uint16_t>(slot);
        world.store(bag,slot,*item);result.bag=bag;result.slot=slot;
        if(definition->server!=0 && definition->required_wisdom>0 && definition->required_wisdom<100) set_ground_item_craft(*item,"00000000000000000000");
        world.send_database(inventory_add_packet(user.billing_id,static_cast<std::uint8_t>(bag),static_cast<std::uint8_t>(slot),*item));return true;
    }
    return false;
}
bool quest_start_items_grant(std::int32_t type,std::int32_t id,std::int32_t count,
    QuestStartItemsUser& user,QuestStartItemsWorld& world) {
    QuestStartItemsResult result;
    if(!quest_start_items_add(type,id,count,result,user,world)) return false;
    PlayerItem receipt;receipt.type=static_cast<std::uint8_t>(type);receipt.type_id=static_cast<std::uint8_t>(id);receipt.count=static_cast<std::uint8_t>(count);receipt.quality=result.definition?result.definition->quality[0]:0;
    world.send_self(inventory_receipt_packet(static_cast<std::uint8_t>(result.bag),static_cast<std::uint8_t>(result.slot),receipt,true));return true;
}
}
