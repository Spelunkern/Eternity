#include "eternity/game/mob_drops_creation.hpp"
#include "eternity/game/ground_items.hpp"
#include <bit>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace eternity::game;
namespace {
void check(bool value,const std::string& why) {if(!value) throw std::runtime_error(why);}
std::string unhex(const std::string& text) {check(text.size()%2==0,"Invalid oracle hex");std::string result;for(std::size_t i=0;i<text.size();i+=2) result+=static_cast<char>(std::stoul(text.substr(i,2),nullptr,16));return result;}
std::string payload(const GroundItem& item) {
    std::string result;const auto add=[&](std::uint64_t value,unsigned count) {for(unsigned i=0;i<count;++i) result+=static_cast<char>(value>>(i*8));};
    add(item.item.uid,8);add(item.item.type,1);add(item.item.type_id,1);add(item.item.count,1);add(item.item.quality,2);
    for(auto value:item.item.gems) add(value,1);for(auto value:item.craft_storage) add(value,1);add(item.item.made_at,4);add(item.item.make_type.empty()?0:static_cast<unsigned char>(item.item.make_type[0]),1);
    add(item.user_id,4);add(item.party_id,4);add(item.drop_type,1);add(item.drop_id,4);add(item.money,4);for(auto value:item.craft_additions) add(value,2);
    add(std::bit_cast<std::uint32_t>(item.position.x),4);add(std::bit_cast<std::uint32_t>(item.position.y),4);add(std::bit_cast<std::uint32_t>(item.position.z),4);add(item.id,4);add(item.definition?1:0,1);return result;
}
void stale(GroundItem& item,const ItemDefinition* definition) {
    item.definition=definition;item.item.uid=0xa5a5a5a5a5a5a5a5ULL;item.item.type=0xa5;item.item.type_id=0xa5;item.item.count=0xa5;item.item.quality=0xa5a5;item.item.gems.fill(0xa5);
    set_ground_item_craft(item,"abcdefghijklmnopqrst");item.item.made_at=0xa5a5a5a5;item.item.make_type.assign(1,static_cast<char>(0xa5));
    item.user_id=0xa5a5a5a5;item.party_id=0xa5a5a5a5;item.drop_type=0xa5;item.drop_id=0xa5a5a5a5;item.money=0xa5a5a5a5;
    for(unsigned i=0;i<11;++i) item.craft_additions[i]=static_cast<std::uint16_t>(100+i);item.position={1.25F,2.5F,3.75F};item.id=0x12345678;
}
struct Random:RandomSource {
    MicrosoftRandom generator;MobDropCreationContext& context;bool mutate=false;unsigned calls=0;
    Random(std::uint32_t seed,MobDropCreationContext& ctx):generator(seed),context(ctx) {}
    std::int32_t next() override {const auto value=generator.next();++calls;if(mutate && calls==1) {context.x=11;context.z=22;context.mob_id=999;}return value;}
};
struct World:MobDropCreationWorld {
    ItemDefinition& definition;MobDropCreationContext& context;std::uint32_t time;std::uint64_t raw;bool mutate;std::string events;
    World(ItemDefinition& def,MobDropCreationContext& ctx,std::uint32_t t,std::uint64_t uid,bool m):definition(def),context(ctx),time(t),raw(uid),mutate(m) {}
    std::uint32_t make_time() override {events+='T';if(mutate) definition.quality[0]=static_cast<std::uint16_t>(definition.quality[0]+7);return time;}
    std::uint64_t raw_unique_id() override {events+='U';if(mutate) context.server_id=static_cast<std::uint8_t>(context.server_id+1);return raw;}
};
struct Zone:ZoneItemServices {
    GroundItemPool& pool;explicit Zone(GroundItemPool& value):pool(value) {}
    std::vector<eternity::net::Packet> packets;unsigned frees=0;
    std::uint32_t tick_ms() override {return 1000;}
    void send_cell_view(CellCoordinates,const eternity::net::Packet& packet) override {packets.push_back(packet);}
    void release(GroundItem& item) override {++frees;pool.release(item);}
};
}
int main(int argc,char** argv) {try {
    check(argc==2,"Expected original creation fixture");std::ifstream input(argv[1]);check(input.good(),"Cannot read fixture");unsigned crafts=0,items=0,money=0,failures=0;
    for(std::string line;std::getline(input,line);) {
        if(line.empty() || line[0]=='#') continue;std::istringstream row(line);char tag;unsigned scenario,have,allocation,server,wis,real,quality,server_id,mob,mutate,expected_calls;
        std::uint32_t seed,owner,party,time,x,z,final_seed;std::uint64_t raw;std::string expected,events;
        row>>tag>>scenario>>seed>>have>>allocation>>server>>wis>>real>>quality>>owner>>party>>server_id>>raw>>time>>x>>z>>mob>>mutate>>expected>>final_seed>>expected_calls>>events;check(!row.fail(),"Malformed creation scenario");
        ItemDefinition definition;definition.id=123;definition.type=1;definition.type_id=2;definition.server=static_cast<std::uint8_t>(server);definition.required_wisdom=static_cast<std::uint16_t>(wis);definition.real_type=static_cast<std::uint8_t>(real);definition.quality[0]=static_cast<std::uint16_t>(quality);
        MobDropCreationContext context{static_cast<std::uint16_t>(mob),static_cast<std::uint8_t>(server_id),std::bit_cast<float>(x),std::bit_cast<float>(z)};Random random(seed,context);random.mutate=mutate;World world(definition,context,time,raw,mutate);
        GroundItem direct;GroundItem* actual=nullptr;
        GroundItemPool pool(0x12345677,allocation?GroundItemPool::Allocator{}:GroundItemPool::Allocator{[]{return std::unique_ptr<GroundItem>{};}});
        if(tag=='C') {stale(direct,have?&definition:nullptr);create_mob_drop_craft(direct,random);actual=&direct;++crafts;}
        else {
            if(allocation) {auto* retained=pool.allocate();check(retained,"Cannot prime real item pool");stale(*retained,have?&definition:nullptr);pool.release(*retained);}
            if(tag=='I') {actual=create_mob_drop_item(context,definition,owner,party,pool,random,world);++items;}
            else {check(tag=='M',"Unknown creation fixture tag");actual=create_mob_drop_money(context,owner,123456,pool,random);++money;}
            check((actual!=nullptr)==(allocation!=0),"Original allocation failure differs");if(!actual) ++failures;
        }
        if(actual) {
            const auto bytes=payload(*actual),reference=unhex(expected);
            if(bytes!=reference) {
                std::string differences;for(std::size_t i=0;i<std::min(bytes.size(),reference.size());++i) if(bytes[i]!=reference[i]) differences+=" "+std::to_string(i)+":"+std::to_string(static_cast<unsigned char>(bytes[i]))+"/"+std::to_string(static_cast<unsigned char>(reference[i]));
                throw std::runtime_error("Original creation payload differs: "+std::string(1,tag)+" "+std::to_string(scenario)+differences);
            }
        }
        check(random.generator.state()==final_seed && random.calls==expected_calls,"Original creation random consumption differs: "+std::string(1,tag)+" "+std::to_string(scenario));
        check((world.events.empty()?"-":world.events)==events,"Original creation time/UID order differs");
    }
    // Compose real pool allocation, craft generation and real zone entry/removal.
    GroundItemPool pool;ItemDefinition definition;definition.id=1;definition.type=1;definition.type_id=1;definition.server=9;definition.required_wisdom=99;definition.real_type=16;
    MobDropCreationContext context{7,3,80,80};Random random(1,context);World world(definition,context,99,0x10000,false);auto* item=create_mob_drop_item(context,definition,42,77,pool,random,world);
    const MapGeometry geometry(81);Zone services(pool);{ZoneItems zone(geometry,services);check(item && zone.enter(*item,2),"Created mob item could not enter real zone");check(zone.find(item->id)==item && services.packets.size()==1 && services.packets[0]==ground_item_appearance(*item,2),"Created drop did not reach real cell appearance");zone.update(61001);check(zone.size()==0 && services.frees==1,"Created drop did not expire through real zone");}
    check(pool.allocate()==item,"Mob drop was not reusable through shared pool");
    check(crafts==768 && items==512 && money==128 && failures>0,"Incomplete creation oracle coverage");std::cout<<crafts<<" original craft calls; "<<items<<" original item and "<<money<<" money creations matched; "<<failures<<" allocation failures; real zone/pool composition passed\n";
} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}}
