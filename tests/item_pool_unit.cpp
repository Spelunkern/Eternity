#include "eternity/game/item_pool.hpp"
#include "eternity/game/equipment.hpp"
#include <array>
#include <algorithm>
#include <barrier>
#include <bit>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <thread>

using namespace eternity;using namespace eternity::game;
namespace {
void require(bool value,const std::string& message) {if(!value) throw std::runtime_error(message);}
std::string hex(const auto& data) {static constexpr char digits[]="0123456789abcdef";std::string value;for(auto byte:data) {value+=digits[byte>>4];value+=digits[byte&15];}return value;}
std::array<std::uint8_t,21> unhex(const std::string& value) {require(value.size()==42,"Invalid item craft buffer");std::array<std::uint8_t,21> result{};for(std::size_t i=0;i<21;++i) result[i]=static_cast<std::uint8_t>(std::stoul(value.substr(2*i,2),nullptr,16));return result;}
std::array<std::uint32_t,11> scalars(const GroundItem& item) {return {item.id,item.expires_at,item.user_id,item.party_id,item.drop_id,item.money,std::bit_cast<std::uint32_t>(item.position.x),std::bit_cast<std::uint32_t>(item.position.y),std::bit_cast<std::uint32_t>(item.position.z),static_cast<std::uint32_t>(item.item.uid),static_cast<std::uint32_t>(item.item.uid>>32)};}
void mutate_scalars(GroundItem& item,const std::array<std::uint32_t,11>& values) {
    item.id=values[0];item.expires_at=values[1];item.user_id=values[2];item.party_id=values[3];item.drop_id=values[4];item.money=values[5];
    item.position={std::bit_cast<float>(values[6]),std::bit_cast<float>(values[7]),std::bit_cast<float>(values[8])};item.item.uid=values[9] | (static_cast<std::uint64_t>(values[10])<<32);
}
struct Services final:ZoneItemServices {
    GroundItemPool& pool;std::vector<net::Packet> packets;
    explicit Services(GroundItemPool& value):pool(value) {}
    std::uint32_t tick_ms() override {return 100;}
    void send_cell_view(CellCoordinates,const net::Packet& packet) override {packets.push_back(packet);}
    void release(GroundItem& item) override {pool.release(item);}
};
}
int main(int argc,char** argv) {
    try {
        require(argc==2,"Expected item pool oracle");std::ifstream input(argv[1]);require(input.good(),"Cannot open item pool oracle");
        std::array<ItemDefinition,4> definitions{};const std::array<std::uint16_t,4> qualities{0,1,12345,65535};for(std::size_t i=0;i<4;++i) definitions[i].quality[0]=qualities[i];
        std::unique_ptr<GroundItemPool> pool;std::vector<GroundItem*> objects;std::set<int> available;std::vector<std::string> events;bool fail=false;
        std::string line;unsigned int count=0;
        while(std::getline(input,line)) {
            if(line.empty() || line[0]=='#') continue;
            std::istringstream row(line);char op=0;row>>op;events.clear();int result=-1;
            if(op=='N') {
                std::uint32_t serial=0;row>>serial;objects.clear();available.clear();pool=std::make_unique<GroundItemPool>(serial,[&]() -> std::unique_ptr<GroundItem> {
                    events.push_back("N"+std::to_string(fail));if(fail) return nullptr;auto item=std::make_unique<GroundItem>();objects.push_back(item.get());return item;
                });
            } else if(op=='A') {
                row>>fail;const auto before=pool->statistics().serial;auto* item=pool->allocate();
                if(item) {const auto entry=std::find(objects.begin(),objects.end(),item);require(entry!=objects.end(),"Pool returned an unknown allocation");result=static_cast<int>(entry-objects.begin());available.erase(result);}
                const auto after=pool->statistics().serial;if(before!=after) events.push_back("I"+std::to_string(after));
            } else if(op=='F') {int index=0;row>>index;pool->release(index>=0?objects.at(static_cast<std::size_t>(index)):nullptr);if(index>=0) available.insert(index);}
            else if(op=='R') {unsigned int index=0;row>>index;initialize_ground_item(*objects.at(index));}
            else if(op=='V') {
                unsigned int index=0,kind=0,type_id=0,amount=0,make_type=0;std::uint32_t made_at=0;row>>index>>kind>>type_id>>amount>>made_at>>make_type;
                initialize_ground_item(*objects.at(index),static_cast<std::uint8_t>(kind),static_cast<std::uint8_t>(type_id),static_cast<std::uint8_t>(amount),made_at,static_cast<std::uint8_t>(make_type));
            } else {
                require(op=='M',"Unknown item pool operation");unsigned int index=0;row>>index;auto& item=*objects.at(index);std::array<std::uint32_t,11> values{};for(auto& value:values) row>>value;mutate_scalars(item,values);
                unsigned int definition=0,kind=0,type_id=0,amount=0,quality=0;row>>definition>>kind>>type_id>>amount>>quality;item.definition=definition?&definitions.at(definition-1):nullptr;
                item.item.type=static_cast<std::uint8_t>(kind);item.item.type_id=static_cast<std::uint8_t>(type_id);item.item.count=static_cast<std::uint8_t>(amount);item.item.quality=static_cast<std::uint16_t>(quality);
                for(auto& gem:item.item.gems) {unsigned int value=0;row>>value;gem=static_cast<std::uint8_t>(value);}
                std::string craft;unsigned int make_type=0,drop_type=0;row>>craft>>item.item.made_at>>make_type>>drop_type;item.craft_storage=unhex(craft);
                const auto end=std::find(item.craft_storage.begin(),item.craft_storage.end(),0);item.item.craft.assign(item.craft_storage.begin(),end);
                item.item.make_type=make_type?std::string(1,static_cast<char>(make_type)):std::string{};item.drop_type=static_cast<std::uint8_t>(drop_type);
                for(auto& addition:item.craft_additions) {unsigned int value=0;row>>value;addition=static_cast<std::uint16_t>(value);}
            }
            const auto context=" at item pool vector "+std::to_string(count);int original_result=0;std::uint32_t serial=0,fresh=0;std::size_t free_count=0,owned=0;row>>original_result>>serial>>fresh>>free_count>>owned;
            const auto statistics=pool->statistics();require(result==original_result && statistics.serial==serial && statistics.fresh_attempts==fresh && statistics.available==free_count && statistics.owned==owned && objects.size()==owned && available.size()==free_count,"Item pool result/statistics differ"+context);
            const auto check=[&](std::uint32_t value,const char* field) {std::uint32_t original=0;row>>original;require(value==original,std::string("Item pool ")+field+" differs"+context);};
            for(std::size_t index=0;index<objects.size();++index) {
                const auto& item=*objects[index];require(!item.zone && !item.cell,"Item pool altered world binding");check(available.contains(static_cast<int>(index)),"free membership");for(auto value:scalars(item)) check(value,"retained scalar");
                check(item.definition?static_cast<std::uint32_t>(item.definition-definitions.data()+1):0,"definition");check(item.item.type,"type");check(item.item.type_id,"type ID");check(item.item.count,"count");check(item.item.quality,"quality");for(auto value:item.item.gems) check(value,"gem");
                std::string original_craft;row>>original_craft;require(hex(item.craft_storage)==original_craft,"Item craft storage differs"+context);const auto end=std::find(item.craft_storage.begin(),item.craft_storage.end(),0);require(item.item.craft==std::string(item.craft_storage.begin(),end),"Item logical/raw craft names diverged");
                check(item.item.made_at,"creation time");check(item.item.make_type.empty()?0:static_cast<std::uint8_t>(item.item.make_type[0]),"creation type");check(item.drop_type,"drop type");for(auto value:item.craft_additions) check(value,"craft addition");
            }
            std::size_t event_count=0;row>>event_count;require(events.size()==event_count,"Item pool event count differs"+context);for(const auto& event:events) {std::string original;row>>original;require(event==original,"Item pool event differs"+context);}
            require(static_cast<bool>(row),"Truncated item pool vector");std::string extra;require(!(row>>extra),"Trailing item pool data");++count;
        }
        require(count==640,"Item pool coverage changed");
        // Concrete zone expiry returns allocations to the actual LIFO pool, retaining
        // metadata/IDs while resetting payload when allocated again.
        GroundItemPool heap;Services services(heap);const MapGeometry geometry(160);ZoneItems zone(geometry,services);auto* first=heap.allocate();auto* second=heap.allocate();const auto first_id=first->id,second_id=second->id;
        first->position={10,20,10};second->position={10,20,10};second->money=321;second->user_id=99;second->item.uid=0x123456789abcdef0ULL;second->definition=&definitions[3];second->craft_additions.fill(65535);set_ground_item_craft(*second,"ABCDEFGHIJKLMNOPQRST");
        zone.enter(*first,1);zone.enter(*second,1);bool invalid=false;try {heap.release(first);}catch(const std::invalid_argument&) {invalid=true;}require(invalid,"Pool accepted an attached item");
        zone.update(60100);require(heap.statistics().available==0,"Pool received an item at equal expiry");zone.update(60101);require(heap.statistics().available==2 && zone.size()==0 && services.packets.size()==4,"Concrete item expiry did not return both allocations");
        auto* reused=heap.allocate();require(reused==second && reused->id==second_id && reused->money==321 && reused->user_id==99 && reused->expires_at==60100 && reused->item.uid==0x123456789abcdef0ULL && reused->definition==&definitions[3] && reused->item.craft.empty() && reused->craft_storage[1]=='B' && reused->craft_additions[10]==0,"Pool reuse lost retained data or kept cleared payload");
        initialize_ground_item(*reused,16,2,3,1234,'U');require(reused->item.quality==65535 && reused->item.count==3 && reused->item.made_at==1234,"Typed item initialization did not use its existing definition");
        set_ground_item_craft(*reused,"x");require(reused->craft_storage[0]=='x' && reused->craft_storage[1]==0 && reused->craft_storage[2]=='C',"Craft update cleared retained tail bytes");
        require(heap.allocate()==first && first->id==first_id,"Pool did not preserve LIFO allocation IDs");heap.release(first);invalid=false;try {heap.release(first);}catch(const std::invalid_argument&) {invalid=true;}require(invalid,"Pool accepted a double release");
        GroundItem foreign;invalid=false;try {heap.release(foreign);}catch(const std::invalid_argument&) {invalid=true;}require(invalid,"Pool accepted a foreign allocation");
        GroundItemPool failing(1,[]() -> std::unique_ptr<GroundItem> {throw std::bad_alloc();});require(failing.allocate()==nullptr && failing.statistics().fresh_attempts==1 && failing.statistics().serial==1,"Native allocation failure changed ID/counter behavior");
        GroundItemPool concurrent;std::array<std::array<GroundItem*,64>,4> held{};std::barrier ready(4);std::array<std::thread,4> threads;
        for(std::size_t i=0;i<4;++i) threads[i]=std::thread([&,i] {for(auto& item:held[i]) item=concurrent.allocate();ready.arrive_and_wait();for(auto* item:held[i]) concurrent.release(item);});for(auto& thread:threads) thread.join();
        std::set<std::uint32_t> identities;for(const auto& batch:held) for(const auto* item:batch) identities.insert(item->id);require(identities.size()==256 && concurrent.statistics().owned==256 && concurrent.statistics().available==256 && concurrent.statistics().serial==257,"Concurrent item pool allocations reused live IDs or lost objects");
        std::cout<<count<<" original item pool/init cases, concrete zone recycling and concurrent allocations passed\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
