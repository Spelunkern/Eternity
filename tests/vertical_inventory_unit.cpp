#define main inventory_replica_fixture_main
#include "inventory_replica_flow_unit.cpp"
#undef main
#include "eternity/game/vertical_inventory.hpp"
#include <bit>
#include <fstream>
#include <sstream>

namespace {
struct Actions final:VerticalInventoryActions {
    std::vector<std::uint8_t>& packet;ZoneItems& zone;std::uint32_t replacement;unsigned mutation;bool flip;std::string events;
    Actions(std::vector<std::uint8_t>& p,ZoneItems& z,std::uint32_t r,unsigned m,bool f):packet(p),zone(z),replacement(r),mutation(m),flip(f) {}
    bool pickup_checks() override {require(events=="C,S","Pickup checks sampled before callbacks");return true;}
    void change() {for(unsigned i=0;i<4;++i) packet[2+i]=static_cast<std::uint8_t>(replacement>>(i*8));}
    void cancel_action_exchange(Actor& actor) override {require(zone.size()==1,"Pickup before action cancellation");events="C";if(mutation==1) change();if(flip) actor.attack_state().mode=1;}
    void end_shop(Actor&) override {require(events=="C" && zone.size()==1,"Shop ended out of order");events+=",S";if(mutation==2) change();}
};
}
int main(int argc,char** argv) {try {
    require(argc==2,"Expected original request oracle");std::ifstream input(argv[1]);require(input.good(),"Cannot open request oracle");
    const ItemDefinitions definitions;const EnchantValues enchant;const MobAttackTemplates mobs;const MapGeometry geometry(160);const ItemQuestLinks links;
    unsigned count=0;
    for(std::string line;std::getline(input,line);) {
        if(line.empty() || line[0]=='#') continue;
        std::istringstream row(line);std::uint32_t mode,id,replacement;unsigned mutation,flip;std::string expected;row>>mode>>id>>mutation>>replacement>>flip>>expected;require(!row.fail(),"Invalid request oracle");
        GroundItemPool pool;InventoryReplicaPool cached;InventoryReplica replica(12345,0,cached);World world(replica,pool);ZoneItems zone(geometry,world);world.zone=&zone;
        Actor actor(9001,{}, {},{},world,{definitions,enchant,mobs});actor.account_id()=12345;actor.attack_state().mode=std::bit_cast<std::int32_t>(mode);
        auto* item=pool.allocate();require(item,"Item allocation failed");item->id=mutation?replacement:id;item->item.type=26;item->item.count=1;item->money=1;item->position={10,0,10};require(zone.enter(*item,1),"Cannot enter item");
        std::vector<std::uint8_t> packet{5,2,0,0,0,0};for(unsigned i=0;i<4;++i) packet[2+i]=static_cast<std::uint8_t>(id>>(8*i));Actions actions(packet,zone,replacement,mutation,flip);
        const auto result=vertical_inventory_request(actor,packet,zone,links,world,pool,actions);
        std::string actual=actions.events;
        if(result.request==VerticalInventoryRequest::pickup) {
            require(result.receipt==InventoryReceipt::money && zone.size()==0 && actor.money()==1 && replica.money()==1,"Real Actor pickup and replica failed");
            actual+=",P"+std::to_string(mutation?replacement:id);
        } else {require(result.request==VerticalInventoryRequest::ignored && !result.receipt && zone.size()==1,"Gate failed");require(zone.leave(*item),"Cannot detach gated item");pool.release(*item);}
        require((actual.empty()?"-":actual)==expected,"Original request trace differs");++count;
    }
    require(count==512,"Missing original request cases");
    GroundItemPool pool;InventoryReplicaPool cached;InventoryReplica replica(12345,0,cached);World world(replica,pool);ZoneItems zone(geometry,world);world.zone=&zone;Actor actor(9001,{}, {},{},world,{definitions,enchant,mobs});std::vector<std::uint8_t> packet{5,2,0,0,0,0};Actions actions(packet,zone,0,0,false);
    for(unsigned length=0;length<6;++length) {const auto result=vertical_inventory_request(actor,std::span(packet).first(length),zone,links,world,pool,actions);require(result.request==VerticalInventoryRequest::malformed && !result.receipt && actions.events.empty(),"Truncated request caused effects");}
    packet[0]=6;require(vertical_inventory_request(actor,packet,zone,links,world,pool,actions).request==VerticalInventoryRequest::unhandled && actions.events.empty(),"Unrelated opcode caused effects");
    auto* other=pool.allocate();other->id=123;other->position={10,0,10};require(zone.enter(*other,1),"Cannot enter rejection item");
    packet={5,2,124,0,0,0};actor.account_id()=12345;
    const auto missing=vertical_inventory_request(actor,packet,zone,links,world,pool,actions);
    require(missing.request==VerticalInventoryRequest::pickup && missing.receipt==InventoryReceipt::rejected && actions.events=="C,S" && zone.size()==1,"Missing-item rejection or prefix ordering changed");
    require(!world.client.empty() && world.client.back()==inventory_failure_packet(3),"Missing-item failure transport changed");
    require(zone.leave(*other),"Cannot detach rejection item");pool.release(*other);
    std::cout<<count<<" original request traces and real Actor/zone/replica pickup compositions passed\n";
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}}
