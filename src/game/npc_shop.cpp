#include "eternity/game/npc_shop.hpp"
#include "eternity/game/game_log.hpp"
#include "eternity/game/inventory.hpp"
#include "eternity/game/item_pool.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace eternity::game {
namespace {
constexpr std::uint8_t shop_make_type=0x42; // 'B', pushed at 0046b8a8 / 0046bb1f.
constexpr std::int32_t no_shop_sale_real_type=0x1b; // stItemInfo nRealType checked at 0046aff0.
constexpr std::uint16_t bag_slots=24;
void put(net::Packet& packet,std::size_t offset,std::uint64_t value,unsigned int length) {
    for(unsigned int i=0;i<length;++i) packet[offset+i]=static_cast<std::uint8_t>(value>>(8*i));
}
void text(net::Packet& packet,std::size_t offset,const std::string& value,std::size_t maximum) {
    const auto used=std::min(value.size(),maximum);std::copy_n(value.begin(),used,packet.begin()+static_cast<std::ptrdiff_t>(offset));
}
net::Packet shop_reply(std::uint16_t opcode,std::uint8_t result,std::uint8_t bag,std::uint8_t slot,std::uint8_t type,
    std::uint8_t type_id,std::uint8_t count,std::uint32_t money) {
    net::Packet packet(12,0);put(packet,0,opcode,2);packet[2]=result;packet[3]=bag;packet[4]=slot;packet[5]=type;packet[6]=type_id;packet[7]=count;put(packet,8,money,4);return packet;
}
// 0404 and 0403 share their layout through the money field. Both follow SetGameLogMain.
net::Packet shop_log(std::uint16_t opcode,std::size_t size,const Actor& actor,const GroundItem& item,std::uint32_t npc,
    std::uint8_t count,std::uint32_t amount,NpcShopWorld& world) {
    net::Packet log(size,0);write_game_log_main(log,world.money_log_subject(actor),world.money_local_calendar());
    put(log,0,opcode,2);put(log,74,item.item.uid,8);put(log,82,item.definition->id,4);text(log,86,world.shop_item_name(*item.definition),31);
    std::copy(item.item.gems.begin(),item.item.gems.end(),log.begin()+118);put(log,124,item.item.made_at,4);
    put(log,149,npc,4);log[153]=count;put(log,154,amount,4);put(log,158,actor.money(),4);return log;
}
// A value-only mirror row occupies its slot but has no original CItem to inspect.
GroundItem* held_item(Actor& actor,std::int32_t bag,std::uint16_t slot) {
    const auto b=static_cast<std::uint8_t>(bag);const auto held=actor.retained_inventory_item(b,slot);
    if(held)return held->item;
    if(!actor.inventory_slot_empty(b,slot))throw std::logic_error("Shop slot holds an item without a retained owner");
    return nullptr;
}
std::int32_t shop_bags(const Actor& actor) {
    const auto bags=actor.status().skills.max_bags;
    if(bags>9)throw std::out_of_range("Original inventory bag count exceeds storage");
    return bags;
}
void finish_buy(Actor& actor,const NpcShopEntity& npc,const GroundItem& item,std::uint8_t bag,std::uint8_t slot,std::uint8_t logged,
    std::uint32_t price,NpcShopWorld& world) {
    actor.send_self(shop_reply(0x702,0,bag,slot,item.item.type,item.item.type_id,item.item.count,actor.money()));
    world.send_money_log(shop_log(0x403,0xa2,actor,item,npc.type_id,logged,price,world));
}
NpcShopResult buy(Actor& actor,const net::Packet& packet,NpcShopWorld& world) {
    if(packet.size()<8)return NpcShopResult::ignored; // Native bound; the original trusts its frame buffer.
    const std::uint32_t id=packet[2]|packet[3]<<8|packet[4]<<16|static_cast<std::uint32_t>(packet[5])<<24;
    const auto index=packet[6];const auto count=packet[7];
    const auto npc=world.find_zone_npc(actor,id);
    if(!npc || npc->kind!=1)return NpcShopResult::ignored;
    const auto* shop=world.find_shop_npc(npc->type_id);
    if(!shop || index>=shop->shop_items.size())return NpcShopResult::ignored;
    const auto* definition=world.find_shop_item(shop->shop_items[index][0],shop->shop_items[index][1]);
    if(!definition || count>definition->stack_count || count==0)return NpcShopResult::ignored;
    world.cancel_action_exchange(actor);world.end_my_shop(actor);
    std::uint32_t price=count*definition->buy;
    if(world.guild_house_zone(actor)) {
        const auto rate=world.guild_house_buy_rate(actor,shop->type_id(),shop->shop_type);
        if(!rate || rate->result!=0) {
            net::Packet rejected(4,0);put(rejected,0,0xd28,2);rejected[2]=rate?static_cast<std::uint8_t>(rate->result):1;rejected[3]=rate?rate->detail:0x1e;
            actor.send_self(rejected);return NpcShopResult::guild_rejected;
        }
        // 0046b49b..0046b4e1: FISTP truncates to 64 bits and IMUL keeps the low DWORD.
        const auto scaled=static_cast<std::int64_t>(static_cast<double>(definition->buy)*rate->rate);
        price=static_cast<std::uint32_t>(scaled)*count;
    }
    if(actor.money()<price) {
        actor.send_self(shop_reply(0x702,1,0,0,definition->type,definition->type_id,count,0));return NpcShopResult::no_money;
    }
    const auto bags=shop_bags(actor);
    if(definition->stack_count>1) for(std::int32_t bag=1;bag<=bags;++bag) for(std::uint16_t slot=0;slot<bag_slots;++slot) {
        auto* item=held_item(actor,bag,slot);
        if(!item || item->item.type!=definition->type || item->item.type_id!=definition->type_id ||
           static_cast<unsigned int>(item->item.count)+count>definition->stack_count)continue;
        // Unlike ItemGet, a shop stack must hold the whole request; it never splits.
        const auto b=static_cast<std::uint8_t>(bag);const auto s=static_cast<std::uint8_t>(slot);const auto total=static_cast<std::uint8_t>(item->item.count+count);
        if(!actor.set_inventory_item_count(b,slot,total,item))throw std::logic_error("Shop stack changed during purchase");
        actor.money()-=price;actor.send_money_database(world);
        world.send_shop_database(inventory_count_packet(actor.account_id(),b,s,total));
        finish_buy(actor,*npc,*item,b,s,count,price,world);return NpcShopResult::bought;
    }
    for(std::int32_t bag=1;bag<=bags;++bag) for(std::uint16_t slot=0;slot<bag_slots;++slot) {
        if(held_item(actor,bag,slot))continue;
        auto& pool=world.shop_item_pool();auto* item=pool.allocate();
        if(!item) {actor.send_self(shop_reply(0x702,2,0,0,definition->type,definition->type_id,0,0));return NpcShopResult::no_space;}
        item->definition=definition;
        const std::uint8_t created=definition->stack_count>1?count:1; // Literal 1 at 0046bb32.
        initialize_ground_item(*item,definition->type,definition->type_id,created,world.shop_system_time(),shop_make_type);
        item->item.uid=world.next_shop_item_uid();
        const auto b=static_cast<std::uint8_t>(bag);const auto s=static_cast<std::uint8_t>(slot);
        if(!actor.claim_inventory_item(b,slot,*item,pool))throw std::logic_error("Shop slot changed during purchase");
        initialize_ground_item_craft(*item);
        if(!actor.sync_inventory_item(b,slot,item))throw std::logic_error("Shop item mirror changed during purchase");
        actor.money()-=price;actor.send_money_database(world);
        world.send_shop_database(inventory_add_packet(actor.account_id(),b,s,*item));
        finish_buy(actor,*npc,*item,b,s,item->item.count,price,world);return NpcShopResult::bought;
    }
    actor.send_self(shop_reply(0x702,2,0,0,definition->type,definition->type_id,0,0));return NpcShopResult::no_space;
}
NpcShopResult sell(Actor& actor,const net::Packet& packet,NpcShopWorld& world) {
    if(packet.size()<5)return NpcShopResult::ignored; // Native bound; the original trusts its frame buffer.
    const auto bag=packet[2];const auto slot=packet[3];const auto count=packet[4];
    if(bag==0 || bag>shop_bags(actor) || slot>=bag_slots)return NpcShopResult::ignored;
    auto* item=held_item(actor,bag,slot);
    if(!item || count>item->item.count || count==0)return NpcShopResult::ignored;
    world.cancel_action_exchange(actor);world.end_my_shop(actor);
    const auto* definition=item->definition;
    if(!definition)throw std::logic_error("Shop sale needs the item definition");
    const bool whole=item->item.count==count;
    // Real type 1b pays nothing and, on a partial sale, keeps the stack unchanged
    // while DB-agent still receives the sold count: 0046aff0 and 0046b1eb.
    const std::uint32_t amount=count*definition->sell;
    if(definition->real_type!=no_shop_sale_real_type) {
        if(amount>std::numeric_limits<std::uint32_t>::max()-actor.money())return NpcShopResult::ignored;
        if(!whole && !actor.set_inventory_item_count(bag,slot,static_cast<std::uint8_t>(item->item.count-count),item))
            throw std::logic_error("Shop stack changed during sale");
        actor.money()+=amount;actor.send_money_database(world);
    }
    net::Packet database(9,0);put(database,0,0x702,2);put(database,2,actor.account_id(),4);database[6]=bag;database[7]=slot;database[8]=count;
    world.send_shop_database(database);
    auto log=shop_log(0x404,0xa4,actor,*item,world.shop_interaction_npc_type_id(actor),count,amount,world);
    log[162]=bag;log[163]=slot;text(log,128,item->item.craft,20);
    world.send_money_log(log);
    const auto type=item->item.type;const auto type_id=item->item.type_id;
    const std::uint8_t remaining=whole?0:item->item.count;
    actor.send_self(shop_reply(0x703,0,bag,slot,type,type_id,remaining,actor.money()));
    if(whole && !actor.release_inventory_item(bag,slot,item))throw std::logic_error("Shop item changed during sale");
    return NpcShopResult::sold;
}
}
NpcShopResult npc_shop_packet(Actor& actor,const net::Packet& packet,NpcShopWorld& world) {
    if(packet.size()<2)return NpcShopResult::ignored;
    if(actor.attack_state().mode==1)return NpcShopResult::ignored; // CUser+142c, 0046af3d.
    const auto opcode=static_cast<std::uint16_t>(packet[0]|packet[1]<<8);
    if(opcode==0x702)return buy(actor,packet,world);
    if(opcode==0x703)return sell(actor,packet,world);
    world.close_shop_connection(actor,9);return NpcShopResult::closed;
}
}
