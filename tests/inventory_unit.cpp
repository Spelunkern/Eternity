#include "eternity/game/actor.hpp"
#include "eternity/game/inventory.hpp"
#include "eternity/game/item_pool.hpp"
#include <algorithm>
#include <bit>
#include <fstream>
#include <iostream>
#include <sstream>

using namespace eternity;using namespace eternity::game;
namespace {
void require(bool value,const std::string& message) {if(!value) throw std::runtime_error(message);}
std::string hex(const auto& data) {static constexpr char digits[]="0123456789abcdef";std::string result;for(auto byte:data) {const auto value=static_cast<std::uint8_t>(byte);result+=digits[value>>4];result+=digits[value&15];}return result.empty()?"-":result;}
net::Packet unhex(const std::string& value) {net::Packet result;if(value=="-") return result;for(std::size_t i=0;i<value.size();i+=2) result.push_back(static_cast<std::uint8_t>(std::stoul(value.substr(i,2),nullptr,16)));return result;}
struct Scene final:ActorScene {
    std::vector<std::string>& events;explicit Scene(std::vector<std::string>& value):events(value) {}
    void send_self(const net::Packet& packet) noexcept override {events.push_back("S"+hex(packet));}
    void unexpected() {throw std::runtime_error("Unexpected inventory scene operation");}
    void send_nearby(const net::Packet&) override {unexpected();}void send_cell_view(const net::Packet&) override {unexpected();}
    std::optional<bool> party_kind() const noexcept override {std::terminate();}void send_party(const net::Packet&) noexcept override {std::terminate();}
    void reset_damage() override {unexpected();}std::optional<std::uint32_t> allocate_effect_id() noexcept override {std::terminate();}void release_effect_id(std::uint32_t) noexcept override {std::terminate();}
    std::uint32_t tick_ms() const noexcept override {std::terminate();}void stop_target_mob(std::uint32_t) override {unexpected();}void taunt(std::uint16_t,float) override {unexpected();}
    ZoneUsers* user_zone() override {unexpected();return nullptr;}UserShapeSocial user_shape_social() override {unexpected();return {};}
    void finish_periodic_damage(EffectStatus&,const SkillDefinition&,const EffectSource&,std::uint32_t) override {unexpected();}
};
struct World final:ItemPickupWorld,ZoneItemServices {
    std::vector<std::string>& events;GroundItemPool& pool;ZoneItems* zone=nullptr;int party=-1,change=-2;bool donor=false,mutated=false,free_recorded=false;std::uint32_t tick=100;
    World(std::vector<std::string>& trace,GroundItemPool& heap):events(trace),pool(heap) {}
    void record_free() {if(pool.statistics().available && !free_recorded) {events.push_back("F");free_recorded=true;}}
    std::uint32_t lock_inventory_party(Actor&) override {if(party<0) return 0;const auto token=static_cast<std::uint32_t>(party+1);events.push_back("L"+std::to_string(token));return token;}
    void unlock_inventory_party(std::uint32_t token) noexcept override {record_free();events.push_back("U"+std::to_string(token));}
    std::optional<bool> inventory_party_kind(const Actor&) override {return party<0?std::nullopt:std::optional<bool>{party!=0};}
    void send_inventory_party(Actor&,const net::Packet& packet) override {events.push_back("P"+hex(packet));}
    void database(const net::Packet& packet) {events.push_back("B"+hex(packet));if(!mutated) {mutated=true;if(change!=-2) party=change;}}
    void send_inventory_database(const net::Packet& packet) override {database(packet);}void send_money_database(const net::Packet& packet) override {database(packet);}
    GameLogSubject money_log_subject(const Actor& actor) override {GameLogSubject result;result.account_id=actor.account_id();result.character_id=23456;result.experience=0xffff0000U;result.account_name="inventory-account";result.character_name="InventoryCharacter";result.level=123;result.map=7;result.position={10.25F,-1.5F,10.75F};return result;}
    std::chrono::sys_seconds money_local_calendar() override {events.push_back("T");return std::chrono::sys_days{std::chrono::year{2026}/11/2}+std::chrono::hours{13}+std::chrono::minutes{47}+std::chrono::seconds{59};}
    void send_money_log(const net::Packet& packet) override {events.push_back("G"+hex(packet));}
    std::string inventory_item_name(const ItemDefinition&) override {return "Inventory item";}
    std::string inventory_drop_user_name(Actor&,std::uint32_t id,bool lookup) override {events.push_back("Q"+std::to_string(lookup)+","+std::to_string(id));return donor?"Dropper":"";}
    bool return_inventory_item(Actor&,GroundItem& item) override {events.push_back("R");return zone->enter(item,1);}
    std::uint32_t tick_ms() override {events.push_back("K");return tick;}
    std::uint32_t pick_tick_ms() override {return tick_ms();}
    std::uint32_t pick_party_id(Actor&) override {events.push_back("J");return party<0?0:static_cast<std::uint32_t>(77+party);}
    UserPosition pick_position(const Actor&) override {events.push_back("D");return {10.25F,-1.5F,10.75F};}
    bool pick_quest_active(Actor&,std::uint16_t) override {throw std::runtime_error("Empty quest links searched an active quest");}
    void send_cell_view(CellCoordinates,const net::Packet& packet) override {events.push_back("V"+hex(packet));}
    void release(GroundItem&) override {throw std::runtime_error("Unexpected inventory remainder expiry");}
};
}
int main(int argc,char** argv) {
    try {
        require(argc==2 || argc==3,"Expected inventory oracle and optional pickup mode");const bool pickup=argc==3;require(!pickup || std::string_view(argv[2])=="pickup","Unknown inventory test mode");std::ifstream input(argv[1]);require(input.good(),"Cannot open inventory oracle");const ItemDefinitions definitions;const EnchantValues enchant;const MobAttackTemplates mobs;const MapGeometry geometry(160);const ItemQuestLinks links;
        std::string line;unsigned int count=0;
        while(std::getline(input,line)) {
            if(line.empty() || line[0]=='#') continue;
            std::istringstream row(line);std::int32_t bags=0;unsigned int maximum=0,amount=0,quality=0,make_type=0,drop_type=0,donor=0;std::uint64_t uid=0;std::uint32_t made_at=0,drop_id=0,x=0;std::string gems,craft;
            std::vector<std::string> events;GroundItemPool pool;World world(events,pool);Scene scene(events);ZoneItems zone(geometry,world);world.zone=&zone;
            row>>bags>>maximum>>amount>>quality>>uid>>made_at>>make_type>>gems>>craft>>drop_type>>drop_id>>world.party>>world.change>>donor>>x;world.donor=donor!=0;
            std::size_t rows=0;row>>rows;std::vector<PlayerItem> inventory;
            for(std::size_t i=0;i<rows;++i) {unsigned int bag=0,slot=0,kind=0,number=0;row>>bag>>slot>>kind>>number;PlayerItem value;value.bag=static_cast<std::uint8_t>(bag);value.slot=static_cast<std::uint16_t>(slot);value.type=kind==1?5:6;value.type_id=kind==1?1:2;value.count=static_cast<std::uint8_t>(number);inventory.push_back(value);}
            bool checks=false,present=false;std::uint32_t owner=0,item_party=0,expiry=0,money=0,drop_money=0;std::int32_t amulet=0,gold=0;unsigned int incoming_type=5;
            if(pickup) {row>>checks>>present>>owner>>item_party>>expiry>>world.tick>>money>>amulet>>gold>>incoming_type>>drop_money;for(auto& value:inventory) if(value.type==5) value.type=static_cast<std::uint8_t>(incoming_type);}
            ItemDefinition definition;definition.id=5001;definition.type=static_cast<std::uint8_t>(incoming_type);definition.real_type=incoming_type==27?27:0;definition.type_id=1;definition.stack_count=static_cast<std::uint8_t>(maximum);
            auto* item=pool.allocate();item->definition=&definition;item->item.type=static_cast<std::uint8_t>(incoming_type);item->item.type_id=1;item->item.count=static_cast<std::uint8_t>(amount);item->item.quality=static_cast<std::uint16_t>(quality);item->item.uid=uid;item->item.made_at=made_at;
            item->item.make_type=make_type?std::string(1,static_cast<char>(make_type)):std::string{};const auto gem_bytes=unhex(gems);require(gem_bytes.size()==6,"Invalid inventory gems");std::copy(gem_bytes.begin(),gem_bytes.end(),item->item.gems.begin());const auto name=unhex(craft);set_ground_item_craft(*item,std::string(name.begin(),name.end()));item->drop_type=static_cast<std::uint8_t>(drop_type);item->drop_id=drop_id;item->position={std::bit_cast<float>(x),-1.5F,10.75F};
            Actor actor(9001,{}, {},{},scene,{definitions,enchant,mobs},inventory);actor.account_id()=12345;actor.status().skills.max_bags=bags;
            if(pickup) {actor.money()=money;actor.status().skills.benefits.amulet=amulet;actor.status().skills.benefits.mob_gold=gold;item->user_id=owner;item->party_id=item_party;item->money=drop_money;if(present) {const auto position=item->position;item->position={10.25F,-1.5F,10.75F};require(zone.enter(*item,0),"Cannot attach pickup fixture");item->position=position;}item->expires_at=expiry;}events.clear();
            const auto receipt=pickup?actor.pickup_item(zone,item->id,checks,links,world,pool):actor.receive_item(*item,world,pool);world.record_free();const auto context=" at inventory vector "+std::to_string(count);
            const std::array<std::uint32_t,5> values{static_cast<std::uint32_t>(receipt),item->item.count,static_cast<std::uint32_t>(pool.statistics().available),static_cast<std::uint32_t>(item->zone!=nullptr),item->expires_at};
            for(std::size_t i=0;i<values.size();++i) {std::uint32_t original=0;row>>original;require(values[i]==original,"Inventory admission state differs"+context+" field "+std::to_string(i));}
            if(pickup) {std::uint32_t original_money=0;row>>original_money;require(actor.money()==original_money,"Pickup balance differs"+context);}
            std::size_t final_rows=0;row>>final_rows;require(actor.inventory().size()==final_rows,"Inventory row count differs"+context);
            auto actual=actor.inventory();std::sort(actual.begin(),actual.end(),[](const auto& a,const auto& b){return std::pair{a.bag,a.slot}<std::pair{b.bag,b.slot};});
            for(const auto& value:actual) {
                const std::array<std::uint64_t,7> fields{value.bag,value.slot,value.type,value.type_id,value.count,value.uid,value.quality};
                for(auto field:fields) {std::uint64_t original=0;row>>original;require(field==original,"Inventory row differs"+context);}
                std::string original_gems,original_craft;row>>original_gems>>original_craft;require(hex(value.gems)==original_gems && hex(value.craft)==original_craft,"Inventory payload differs"+context);
            }
            std::size_t event_count=0;row>>event_count;require(events.size()==event_count,"Inventory event count differs"+context+": "+std::to_string(events.size())+" versus "+std::to_string(event_count));
            for(const auto& event:events) {std::string original;row>>original;require(event==original,"Inventory event differs"+context+": "+event+" versus "+original);}
            require(static_cast<bool>(row),"Truncated inventory vector");std::string extra;require(!(row>>extra),"Trailing inventory data");++count;
        }
        require(count==(pickup?2048U:640U),"Inventory oracle coverage changed");
        // A stored allocation remains owned by the actor. Later stack updates
        // change both its projected inventory row and that same pooled allocation.
        std::vector<std::string> events;GroundItemPool pool;World world(events,pool);Scene scene(events);ZoneItems zone(geometry,world);world.zone=&zone;
        ItemDefinition definition;definition.id=5001;definition.type=5;definition.type_id=1;definition.stack_count=10;GroundItem* first=nullptr;GroundItem* last=nullptr;
        {
            Actor actor(9001,{}, {},{},scene,{definitions,enchant,mobs});actor.account_id()=12345;
            const auto item=[&](std::uint8_t number,std::uint64_t uid) {auto* value=pool.allocate();value->definition=&definition;value->item.type=5;value->item.type_id=1;value->item.count=number;value->item.uid=uid;return value;};
            first=item(2,777);first->item.quality=500;require(actor.receive_item(*first,world,pool)==InventoryReceipt::stored && pool.statistics().available==0,"Stored inventory allocation was not retained");
            const auto retained=actor.retained_inventory_item(1,0);require(retained && retained->item==first && retained->pool==&pool && first->item.bag==1 && first->item.slot==0 && first->item.id==first->definition->id,"Stored item ownership and position diverged");
            auto* second=item(3,888);second->item.quality=12345;require(actor.receive_item(*second,world,pool)==InventoryReceipt::merged && first->item.count==5 && actor.inventory()[0].count==5 && first->item.quality==500 && pool.statistics().available==1,"Later stacking lost retained identity/payload");
            last=item(200,999);require(last==second,"Inventory merge did not return its input to the actual LIFO pool");
            require(actor.receive_item(*last,world,pool)==InventoryReceipt::stored && first->item.count==10 && last->item.count==195 && actor.inventory().size()==2 && actor.inventory()[1].slot==1 && actor.inventory()[1].count==195,"Oversized remainder was split or retained counts diverged");
            require(pool.statistics().available==0,"Actor-owned inventory item returned to the pool prematurely");
        }
        require(pool.statistics().available==2 && pool.allocate()==last,"Actor teardown lost retained allocations or their LIFO release order");
        std::cout<<count<<(pickup?" original complete ID pickup cases":" original complete item-admission cases")<<" and consecutive pooled admissions passed\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
