#include "eternity/game/actor.hpp"
#include "eternity/game/inventory.hpp"
#include "eternity/game/inventory_replica.hpp"
#include "eternity/game/item_pool.hpp"
#include "equipment_fixture.hpp"
#include <algorithm>
#include <iostream>

using namespace eternity;using namespace eternity::game;
namespace {
void require(bool value,const char* message) {if(!value) throw std::runtime_error(message);}
struct World final:ActorScene,ItemDropWorld,ItemPickupWorld,ZoneItemServices {
    InventoryReplica& replica;GroundItemPool& pool;ZoneItems* zone=nullptr;std::uint64_t next_uid=3;std::vector<net::Packet> client,view,database,logs;
    World(InventoryReplica& cache,GroundItemPool& items):replica(cache),pool(items) {}
    void unexpected() {throw std::runtime_error("Unexpected inventory flow operation");}
    void send_self(const net::Packet& packet) noexcept override {client.push_back(packet);}
    void send_nearby(const net::Packet& packet) override {view.push_back(packet);}void send_cell_view(const net::Packet& packet) override {view.push_back(packet);}
    std::optional<bool> party_kind() const noexcept override {return std::nullopt;}void send_party(const net::Packet&) noexcept override {std::terminate();}
    void reset_damage() override {unexpected();}std::optional<std::uint32_t> allocate_effect_id() noexcept override {std::terminate();}void release_effect_id(std::uint32_t) noexcept override {std::terminate();}
    std::uint32_t tick_ms() const noexcept override {return 100;}std::uint32_t tick_ms() override {return 100;}
    void stop_target_mob(std::uint32_t) override {unexpected();}void taunt(std::uint16_t,float) override {unexpected();}ZoneUsers* user_zone() override {unexpected();return nullptr;}UserShapeSocial user_shape_social() override {unexpected();return {};}
    void finish_periodic_damage(EffectStatus&,const SkillDefinition&,const EffectSource&,std::uint32_t) override {unexpected();}
    std::uint32_t lock_inventory_party(Actor&) override {return 0;}void unlock_inventory_party(std::uint32_t) noexcept override {std::terminate();}
    std::optional<bool> inventory_party_kind(const Actor&) override {return std::nullopt;}void send_inventory_party(Actor&,const net::Packet&) override {unexpected();}
    void send_inventory_database(const net::Packet& packet) override {database.push_back(packet);require(replica.apply(packet)==InventoryReplicaResult::applied,"Actor inventory message failed in DB-agent cache");}
    void send_money_database(const net::Packet& packet) override {database.push_back(packet);require(replica.apply_money(packet),"Actor money message failed in shared DB-agent cache");}
    GameLogSubject money_log_subject(const Actor& actor) override {GameLogSubject subject;subject.account_id=actor.account_id();subject.character_id=actor.id();subject.character_name="Flow";subject.account_name="flow";subject.position=drop_position(actor);return subject;}
    std::chrono::sys_seconds money_local_calendar() override {return std::chrono::sys_days{std::chrono::year{2026}/11/2};}void send_money_log(const net::Packet& packet) override {logs.push_back(packet);}
    std::string inventory_item_name(const ItemDefinition&) override {return "Flow item";}std::string inventory_drop_user_name(Actor&,std::uint32_t,bool) override {unexpected();return {};}
    bool return_inventory_item(Actor&,GroundItem& item) override {return zone->enter(item,1);}
    Actor* find_exchange_user(Actor&,std::uint32_t) override {unexpected();return nullptr;}void negative_craft_reduction(const PlayerItem&,std::int32_t) override {unexpected();}
    UserPosition drop_position(const Actor&) override {return {10,0,10};}std::uint64_t next_drop_uid() override {return next_uid++;}std::uint32_t drop_make_time() override {return pack_world_time(money_local_calendar());}
    bool enter_dropped_item(Actor&,GroundItem& item) override {return zone->enter(item,1);}
    std::uint32_t pick_tick_ms() override {return 100;}std::uint32_t pick_party_id(Actor&) override {return 0;}UserPosition pick_position(const Actor& actor) override {return drop_position(actor);}bool pick_quest_active(Actor&,std::uint16_t) override {unexpected();return false;}
    void send_cell_view(CellCoordinates,const net::Packet& packet) override {view.push_back(packet);}void release(GroundItem& item) override {pool.release(item);}
};
}
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
