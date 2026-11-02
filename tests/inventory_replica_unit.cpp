#include "eternity/game/inventory_replica.hpp"
#include "eternity/game/money.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>

using namespace eternity;using namespace eternity::game;
namespace {
void require(bool value,const std::string& message) {if(!value) throw std::runtime_error(message);}
net::Packet unhex(const std::string& value) {require(value.size()%2==0,"Bad replica hex");net::Packet result;for(std::size_t i=0;i<value.size();i+=2) result.push_back(static_cast<std::uint8_t>(std::stoul(value.substr(i,2),nullptr,16)));return result;}
std::string hex(const auto& values) {static constexpr char digits[]="0123456789abcdef";std::string result;for(auto byte:values) {const auto value=static_cast<std::uint8_t>(byte);result+=digits[value>>4];result+=digits[value&15];}return result;}
CachedInventoryItem read(std::istream& row) {
    CachedInventoryItem result;auto& item=result.item;unsigned int type=0,tid=0,count=0,quality=0,make=0;std::string gems,craft;
    row>>item.uid>>item.id>>type>>tid>>count>>quality>>gems>>craft>>item.made_at>>make>>result.save_type>>result.saved_type;
    item.type=static_cast<std::uint8_t>(type);item.type_id=static_cast<std::uint8_t>(tid);item.count=static_cast<std::uint8_t>(count);item.quality=static_cast<std::uint16_t>(quality);if(make) item.make_type.assign(1,static_cast<char>(make));const auto g=unhex(gems),c=unhex(craft);require(g.size()==6 && c.size()==21,"Bad replica payload");std::copy(g.begin(),g.end(),item.gems.begin());std::copy(c.begin(),c.end(),result.craft_storage.begin());const auto end=std::find(c.begin(),c.end(),0);item.craft.assign(c.begin(),end);return result;
}
void compare(const CachedInventoryItem& actual,const CachedInventoryItem& expected,const std::string& context) {
    const auto& a=actual.item;const auto& b=expected.item;
    require(a.uid==b.uid && a.id==b.id && a.type==b.type && a.type_id==b.type_id && a.count==b.count && a.quality==b.quality && a.gems==b.gems && a.made_at==b.made_at && a.make_type==b.make_type && a.craft==b.craft && actual.craft_storage==expected.craft_storage && actual.save_type==expected.save_type && actual.saved_type==expected.saved_type,"Cached item differs"+context+" (UID "+std::to_string(a.uid)+" vs "+std::to_string(b.uid)+", count "+std::to_string(a.count)+" vs "+std::to_string(b.count)+", save "+std::to_string(actual.save_type)+" vs "+std::to_string(expected.save_type)+")");
}
}
int main(int argc,char** argv) {
    try {
        require(argc==2,"Expected DB-agent inventory oracle");std::ifstream input(argv[1]);require(input.good(),"Cannot open replica oracle");std::unique_ptr<InventoryReplicaPool> pool;std::unique_ptr<InventoryReplica> replica;bool allocate=true;std::string line;unsigned int checked=0;
        while(std::getline(input,line)) {
            if(line.empty() || line[0]=='#') continue;std::istringstream row(line);unsigned int episode=0,step=0;bool initial=false;std::uint32_t money=0;std::size_t rows=0;row>>episode>>step>>initial>>money>>rows;
            if(initial) {replica.reset();pool=std::make_unique<InventoryReplicaPool>([&]()->std::unique_ptr<CachedInventoryItem>{return allocate?std::make_unique<CachedInventoryItem>():nullptr;});replica=std::make_unique<InventoryReplica>(12345,money,*pool);allocate=true;}
            require(replica!=nullptr,"Persistent replica missing");
            for(std::size_t i=0;i<rows;++i) {bool bank=false;std::size_t index=0;row>>bank>>index;auto loaded=read(row);loaded.item.bag=bank?0:static_cast<std::uint8_t>(index/24);loaded.item.slot=static_cast<std::uint16_t>(bank?index:index%24);require(loaded.saved_type==0 && replica->load(loaded.item,bank,loaded.save_type),"Cannot seed original cache");}
            std::string packet;unsigned int result=0;row>>packet>>allocate>>result;const auto bytes=unhex(packet);const auto actual=replica->apply(bytes);const auto context=" at replica episode "+std::to_string(episode)+", step "+std::to_string(step)+", opcode "+std::to_string(bytes[0]|static_cast<unsigned int>(bytes[1])<<8);
            require(static_cast<unsigned int>(actual)==result,"Replica dispatch result differs"+context);
            for(bool bank:{false,true}) {
                const auto items=bank?replica->bank():replica->inventory();std::size_t count=0;row>>count;require(items.size()==count,"Replica row count differs"+context+" bank="+std::to_string(bank));
                for(const auto& item:items) {std::size_t index=0;row>>index;require(index==(bank?item.item.slot:item.item.bag*24+item.item.slot),"Replica position differs"+context);compare(item,read(row),context);}
            }
            for(const auto* journal:{&replica->deleted_inventory(),&replica->deleted_bank()}) {std::size_t count=0;row>>count;require(journal->size()==count,"Replica delete journal length differs"+context+": "+std::to_string(journal->size())+" vs "+std::to_string(count));for(auto uid:*journal) {std::uint64_t original=0;row>>original;require(uid==original,"Replica FIFO delete journal differs"+context);}}
            std::uint32_t balance=0;std::size_t attempts=0,available=0,stranded=0;row>>balance>>attempts>>available>>stranded;const auto statistics=pool->statistics();require(replica->money()==balance && statistics.attempts==attempts && statistics.available==available && statistics.stranded==stranded,"Replica money/pool state differs"+context+": attempts="+std::to_string(statistics.attempts)+"/"+std::to_string(attempts)+", free="+std::to_string(statistics.available)+"/"+std::to_string(available)+", stranded="+std::to_string(statistics.stranded)+"/"+std::to_string(stranded));
            require(static_cast<bool>(row),"Truncated replica vector");std::string extra;require(!(row>>extra),"Trailing replica vector");++checked;
        }
        require(checked==2048,"Replica oracle coverage changed");replica.reset();pool.reset();
        // The native dispatcher checks ownership, complete lengths and index domains
        // before touching a cache; unrelated billing/guild owners remain explicit.
        InventoryReplicaPool validation_pool;InventoryReplica validation(12345,99,validation_pool);PlayerItem item;item.bag=1;item.slot=0;item.type=5;item.type_id=1;item.count=10;item.uid=1;require(validation.load(item),"Cannot seed validation row");
        for(const net::Packet packet:std::vector<net::Packet>{{2,7},{2,7,0,0,0,0,1,0,1},{2,7,0x39,0x30,0,0,6,0,1},{2,7,0x39,0x30,0,0,1,24,1},{8,7,0x39,0x30,0,0,1,0,6,1,0,0,0,0},{0x17,6,0x39,0x30,0,0,6,0,1},{0x17,6,0x39,0x30,0,0,1,24,1},{0x18,6,0x39,0x30,0,0,240,1},{0x19,6,0x39,0x30,0,0,255},{0x17,6,0x39,0x30,0,0,1,0},{0x18,6,0x38,0x30,0,0,1,1}}) require(validation.apply(packet)==InventoryReplicaResult::rejected,"Malformed/cross-owner cache command accepted");
        require(validation.inventory().size()==1 && validation.inventory()[0].item.count==10 && validation.deleted_inventory().empty() && validation.money()==99,"Rejected commands mutated the cache");
        require(validation.apply({})==InventoryReplicaResult::rejected,"Empty cache command accepted");for(auto opcode:{0x710,0x712,0x713,0x714}) {net::Packet packet{static_cast<std::uint8_t>(opcode),7,0x39,0x30,0,0};require(validation.apply(packet)==InventoryReplicaResult::unsupported,"Other cache owner's opcode was swallowed");}
        require(validation.apply_money(money_database_packet(12345,UINT32_MAX)) && validation.money()==UINT32_MAX,"Money status did not share the inventory cache field");
        require(validation.apply(net::Packet{8,7,0x39,0x30,0,0,1,0,0,7,77,0,0,0})==InventoryReplicaResult::applied && validation.money()==77 && validation.inventory()[0].item.gems[0]==7,"Gem update did not replace that same owned money field");
        require(!validation.apply_money(money_database_packet(12346,0)) && validation.money()==77,"Cross-owner money status changed the item cache");
        std::cout<<checked<<" persistent original DB-agent inventory/bank operations passed\n";
    }catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
