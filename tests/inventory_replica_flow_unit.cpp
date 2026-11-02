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
int main() {
    try {
        auto table=item_definitions();const auto column=std::find(table.columns.begin(),table.columns.end(),"Count")-table.columns.begin();table.rows[0][static_cast<std::size_t>(column)]="10";const auto definitions=ItemDefinitions::load(table);const EnchantValues enchant;const MobAttackTemplates mobs;GroundItemPool items;InventoryReplicaPool cached_items;InventoryReplica replica(12345,100,cached_items);World world(replica,items);const MapGeometry geometry(160);ZoneItems zone(geometry,world);world.zone=&zone;
        Actor actor(9001,{}, {},{},world,{definitions,enchant,mobs});actor.account_id()=12345;actor.money()=100;actor.status().skills.max_bags=1;PlayerItem row;row.bag=1;row.type=5;row.type_id=1;row.count=4;row.uid=1;require(actor.load_inventory_item(row,items) && replica.load(row),"Cannot bind shared SQL-loaded inventory");
        actor.receive_money(50,static_cast<ItemDropWorld&>(world));require(actor.money()==150 && replica.money()==150,"Money credit did not share the inventory status field");
        auto* incoming=items.allocate();incoming->definition=definitions.find(5,1);incoming->item.type=5;incoming->item.type_id=1;incoming->item.count=3;incoming->item.uid=2;
        require(actor.receive_item(*incoming,static_cast<ItemDropWorld&>(world),items)==InventoryReceipt::merged && actor.inventory()[0].count==7 && replica.inventory()[0].item.count==7 && replica.inventory()[0].save_type==2,"Stack receipt did not dirty the loaded DB row");
        require(actor.drop_item(1,0,2,world,items)==ItemDropResult::partial,"Cannot split native stack");auto* ground=zone.find(3);require(ground && ground->item.uid==3 && actor.inventory()[0].count==5 && replica.inventory()[0].item.count==5 && replica.deleted_inventory().empty(),"Partial drop did not preserve source UID/cache count");
        const ItemQuestLinks links;require(actor.pickup_item(zone,ground->id,true,links,world,items)==InventoryReceipt::merged && zone.size()==0 && actor.inventory()[0].count==7 && replica.inventory()[0].item.count==7,"Ground pickup failed through the real replica");
        require(actor.remove_item(1,0,world) && actor.inventory().empty() && replica.inventory().empty() && replica.deleted_inventory()==std::vector<std::uint64_t>{1},"Removal did not journal the persisted UID");
        auto* fresh=items.allocate();fresh->definition=definitions.find(5,1);fresh->item.type=5;fresh->item.type_id=1;fresh->item.count=1;fresh->item.uid=4;
        require(actor.receive_item(*fresh,static_cast<ItemDropWorld&>(world),items)==InventoryReceipt::stored && replica.inventory()[0].item.uid==4 && replica.inventory()[0].save_type==1,"New receipt did not become a DB insert");
        require(actor.remove_item(1,0,world) && replica.deleted_inventory()==std::vector<std::uint64_t>{1} && replica.inventory().empty() && replica.money()==150,"Transient received item incorrectly became a SQL deletion");
        require(items.statistics().available==2 && cached_items.statistics().available==1,"Actor/DB-agent item pools lost ownership during the round trip");
        std::cout<<"Native actor money, stacking, split drop, zone pickup, insertion and deletion reached the original DB-agent cache contracts\n";
    }catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
