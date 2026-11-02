#include "eternity/game/inventory.hpp"
#include "eternity/game/actor.hpp"
#include "eternity/game/item_pool.hpp"
#include <algorithm>
#include <stdexcept>

namespace eternity::game {
namespace {
void put(net::Packet& packet,std::size_t offset,std::uint64_t value,unsigned int length) {for(unsigned int i=0;i<length;++i) packet[offset+i]=static_cast<std::uint8_t>(value>>(8*i));}
void text(net::Packet& packet,std::size_t offset,std::string_view value,std::size_t maximum) {
    if(value.size()>maximum || value.find('\0')!=std::string_view::npos) throw std::invalid_argument("Invalid original inventory wire string");
    std::copy(value.begin(),value.end(),packet.begin()+static_cast<std::ptrdiff_t>(offset));packet[offset+value.size()]=0;
}
void loot(Actor& actor,GroundItem& item,InventoryWorld& world,bool partial) {
    const auto type=item.drop_type;if(type==0 || type>3) return;
    net::Packet packet(type==3?174:151,0);const auto subject=world.money_log_subject(actor);write_game_log_main(packet,subject,world.money_local_calendar());
    put(packet,0,type==3?0x408:0x401,2);put(packet,74,item.item.uid,8);put(packet,82,item.definition->id,4);
    if(type==3) {put(packet,149,item.drop_id,4);text(packet,153,world.inventory_drop_user_name(actor,item.drop_id,partial),20);}
    text(packet,86,world.inventory_item_name(*item.definition),31);std::copy(item.item.gems.begin(),item.item.gems.end(),packet.begin()+118);put(packet,124,item.item.made_at,4);text(packet,128,item.item.craft,20);
    if(type!=3) packet[150]=item.item.count;
    world.send_money_log(packet);
}
void party_notice(Actor& actor,const GroundItem& item,InventoryWorld& world) {
    if(const auto raid=world.inventory_party_kind(actor)) {net::Packet packet(8,0);put(packet,0,*raid?0xc14:0xb08,2);put(packet,2,actor.id(),4);packet[6]=item.item.type;packet[7]=item.item.type_id;world.send_inventory_party(actor,packet);}
}
}
net::Packet inventory_count_packet(std::uint32_t account_id,std::uint8_t bag,std::uint8_t slot,std::uint8_t count) {
    net::Packet packet(9,0);put(packet,0,0x707,2);put(packet,2,account_id,4);packet[6]=bag;packet[7]=slot;packet[8]=count;return packet;
}
net::Packet inventory_add_packet(std::uint32_t account_id,std::uint8_t bag,std::uint8_t slot,const GroundItem& item) {
    // ps_game.exe ItemGet(CItem*) 0045e33a..0045e413: fixed 57-byte DB-agent row.
    if(!item.definition) throw std::invalid_argument("Inventory item needs its definition");
    net::Packet packet(57,0);put(packet,0,0x701,2);put(packet,2,account_id,4);put(packet,6,item.item.uid,8);put(packet,14,item.definition->id,4);packet[18]=bag;packet[19]=slot;packet[20]=item.item.type;packet[21]=item.item.type_id;packet[22]=item.item.count;
    put(packet,23,item.item.quality,2);std::copy(item.item.gems.begin(),item.item.gems.end(),packet.begin()+25);text(packet,31,item.item.craft,20);put(packet,52,item.item.made_at,4);packet[56]=item.item.make_type.empty()?0:static_cast<std::uint8_t>(item.item.make_type[0]);return packet;
}
net::Packet inventory_receipt_packet(std::uint8_t bag,std::uint8_t slot,const PlayerItem& item,bool extended) {
    net::Packet packet(16+(extended?item.craft.size():0),0);put(packet,0,0x205,2);packet[2]=bag;packet[3]=slot;packet[4]=item.type;packet[5]=item.type_id;packet[6]=item.count;
    if(extended) {put(packet,7,item.quality,2);std::copy(item.gems.begin(),item.gems.end(),packet.begin()+9);text(packet,15,item.craft,20);}return packet;
}
InventoryReceipt Actor::receive_item(GroundItem& item,InventoryWorld& world,GroundItemPool& pool) {
    // Complete CUser::ItemGet(CItem*) 0045df40. ID-based selection/distribution
    // is a separate caller. The incoming allocation must already be detached.
    if(item.zone || item.cell) throw std::invalid_argument("Inventory admission requires a detached item");
    if(item.item.type==26) {receive_money_item(item,world,pool);return InventoryReceipt::money;}
    if(!item.definition) throw std::invalid_argument("Inventory admission needs its original definition");
    if(status_.skills.max_bags>9) throw std::out_of_range("Original inventory bag count exceeds storage");
    const auto initial=item.item.count;const auto token=world.lock_inventory_party(*this);
    struct Unlock {InventoryWorld& world;std::uint32_t token;~Unlock() {if(token) world.unlock_inventory_party(token);}} unlock{world,token};
    const auto at=[&](std::int32_t bag,std::uint16_t slot) -> PlayerItem* {const auto found=std::find_if(inventory_.begin(),inventory_.end(),[&](const auto& value){return value.bag==bag && value.slot==slot;});return found==inventory_.end()?nullptr:&*found;};
    const auto set_count=[&](std::int32_t bag,std::uint16_t slot,std::uint8_t count) {
        auto* target=at(bag,slot);if(!target) throw std::logic_error("Inventory stack disappeared");target->count=count;
        for(auto& held:retained_inventory_) if(held.bag==bag && held.slot==slot) held.item->item.count=count;
    };
    const auto finish=[&](bool merged) {party_notice(*this,item,world);loot(*this,item,world,false);if(merged) pool.release(item);return merged?InventoryReceipt::merged:InventoryReceipt::stored;};
    if(item.definition->stack_count>1) for(std::int32_t bag=1;bag<=status_.skills.max_bags;++bag) {
        if(bag>9) throw std::out_of_range("Original inventory bag count exceeds storage");
        for(std::uint16_t slot=0;slot<24;++slot) {
            auto* target=at(bag,slot);if(!target || target->type!=item.item.type || target->type_id!=item.item.type_id || target->count>=item.definition->stack_count) continue;
            const auto sum=static_cast<unsigned int>(target->count)+item.item.count;const auto maximum=item.definition->stack_count;const bool fits=sum<=maximum;
            if(!fits) item.item.count=static_cast<std::uint8_t>(sum-maximum);
            set_count(bag,slot,fits?static_cast<std::uint8_t>(sum):maximum);
            world.send_inventory_database(inventory_count_packet(account_id_,static_cast<std::uint8_t>(bag),static_cast<std::uint8_t>(slot),at(bag,slot)->count));
            const auto* current=at(bag,slot);if(!current) throw std::logic_error("Inventory stack disappeared during DB update");send_self(inventory_receipt_packet(static_cast<std::uint8_t>(bag),static_cast<std::uint8_t>(slot),*current,false));
            if(fits) return finish(true);
        }
    }
    for(std::int32_t bag=1;bag<=status_.skills.max_bags;++bag) {
        if(bag>9) throw std::out_of_range("Original inventory bag count exceeds storage");
        for(std::uint16_t slot=0;slot<24;++slot) if(!at(bag,slot)) {
            auto value=item.item;value.id=item.definition->id;value.bag=static_cast<std::uint8_t>(bag);value.slot=slot;inventory_.push_back(std::move(value));retained_inventory_.push_back({static_cast<std::uint8_t>(bag),slot,&item,&pool});
            world.send_inventory_database(inventory_add_packet(account_id_,static_cast<std::uint8_t>(bag),static_cast<std::uint8_t>(slot),item));
            send_self(inventory_receipt_packet(static_cast<std::uint8_t>(bag),static_cast<std::uint8_t>(slot),item.item,item.definition->stack_count<2));return finish(false);
        }
    }
    if(initial!=item.item.count) {party_notice(*this,item,world);loot(*this,item,world,true);}
    (void)world.return_inventory_item(*this,item);return InventoryReceipt::returned;
}
}
