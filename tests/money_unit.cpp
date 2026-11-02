#include "eternity/game/actor.hpp"
#include "eternity/game/item_pool.hpp"
#include "eternity/game/item_pick.hpp"
#include "eternity/game/money.hpp"
#include <fstream>
#include <iostream>
#include <sstream>

using namespace eternity;using namespace eternity::game;
namespace {
void require(bool value,const std::string& message) {if(!value) throw std::runtime_error(message);}
std::string hex(const net::Packet& data) {static constexpr char digits[]="0123456789abcdef";std::string result;for(auto byte:data) {result+=digits[byte>>4];result+=digits[byte&15];}return result;}
struct Scene final:ActorScene {
    std::vector<std::string>& events;GroundItemPool* pool=nullptr;
    explicit Scene(std::vector<std::string>& trace):events(trace) {}
    void send_self(const net::Packet& packet) noexcept override {if(pool && pool->statistics().available!=0) std::terminate();events.push_back("S"+hex(packet));}
    void unexpected() {throw std::runtime_error("Unexpected money actor operation");}
    void send_nearby(const net::Packet&) override {unexpected();}
    void send_cell_view(const net::Packet&) override {unexpected();}
    std::optional<bool> party_kind() const noexcept override {std::terminate();}
    void send_party(const net::Packet&) noexcept override {std::terminate();}
    void reset_damage() override {unexpected();}
    std::optional<std::uint32_t> allocate_effect_id() noexcept override {std::terminate();}
    void release_effect_id(std::uint32_t) noexcept override {std::terminate();}
    std::uint32_t tick_ms() const noexcept override {std::terminate();}
    void stop_target_mob(std::uint32_t) override {unexpected();}
    void taunt(std::uint16_t,float) override {unexpected();}
    ZoneUsers* user_zone() override {unexpected();return nullptr;}
    UserShapeSocial user_shape_social() override {unexpected();return {};}
    void finish_periodic_damage(EffectStatus&,const SkillDefinition&,const EffectSource&,std::uint32_t) override {unexpected();}
};
struct World final:MoneyWorld,ItemPickWorld,ZoneItemServices {
    std::vector<std::string>& events;Actor* actor=nullptr;GroundItemPool* pool=nullptr;int year=2026;
    UserPosition position{10.25F,-1.5F,100000};
    MoneyReplica replica;
    explicit World(std::vector<std::string>& trace):events(trace) {}
    void send_money_database(const net::Packet& packet) override {require(!pool || pool->statistics().available==0,"Money item freed before DB update");require(packet==money_database_packet(actor->account_id(),actor->money()),"Money DB update did not capture current actor state");require(replica.apply(packet) && replica.money()==actor->money(),"DB-agent money replica did not apply its owned packet");events.push_back("B"+hex(packet));}
    GameLogSubject money_log_subject(const Actor& user) override {
        require(&user==actor,"Wrong money log subject");GameLogSubject result;result.account_id=user.account_id();result.character_id=23456;result.experience=0xffff0000U;result.account_name="money-account";result.character_name="MoneyCharacter";result.level=123;result.map=7;result.position=position;return result;
    }
    std::chrono::sys_seconds money_local_calendar() override {events.push_back("T");return std::chrono::sys_days{std::chrono::year{year}/11/2}+std::chrono::hours{13}+std::chrono::minutes{47}+std::chrono::seconds{59};}
    void send_money_log(const net::Packet& packet) override {require(!pool || pool->statistics().available==0,"Money item freed before game log");events.push_back("G"+hex(packet));}
    std::uint32_t tick_ms() override {return 100;}
    void send_cell_view(CellCoordinates,const net::Packet& packet) override {events.push_back("V"+hex(packet));}
    void release(GroundItem&) override {throw std::runtime_error("Money pickup unexpectedly expired an item");}
    std::uint32_t pick_tick_ms() override {events.push_back("K");return 100;}
    std::uint32_t pick_party_id(Actor&) override {events.push_back("P");return 0;}
    UserPosition pick_position(const Actor&) override {events.push_back("D");return position;}
    bool pick_quest_active(Actor&,std::uint16_t) override {throw std::runtime_error("Money pickup tested a quest");}
};
}
int main(int argc,char** argv) {
    try {
        require(argc==2,"Expected money oracle");std::ifstream input(argv[1]);require(input.good(),"Cannot open money oracle");
        const ItemDefinitions items;const EnchantValues enchant;const MobAttackTemplates mobs;std::string line;unsigned int count=0;
        while(std::getline(input,line)) {
            if(line.empty() || line[0]=='#') continue;
            std::istringstream row(line);unsigned int op=0,type_id=0,amount_count=0;std::uint32_t current=0,amount=0,account=0;std::vector<std::string> events;Scene scene(events);World world(events);GroundItemPool pool;
            row>>op>>current>>amount>>account>>world.year>>type_id>>amount_count;Actor actor(9001,{}, {},{},scene,{items,enchant,mobs});world.actor=&actor;actor.account_id()=account;actor.money()=current;world.replica=MoneyReplica(account,current);
            std::uint32_t result=0,accepted=0;const auto initial_amount=amount;
            if(op==0) result=money_addable(actor.money(),amount);
            else if(op==1) result=money_sufficient(actor.money(),amount);
            else if(op==2) add_received_money(actor.money(),amount);
            else if(op==3) add_money(actor.money(),amount);
            else if(op==4) actor.send_money_database(world);
            else if(op==5) accepted=actor.receive_money(amount,world);
            else {
                require(op==6,"Unknown money operation");auto* item=pool.allocate();require(item!=nullptr,"Money item allocation failed");item->item.type=26;item->item.type_id=static_cast<std::uint8_t>(type_id);item->item.count=static_cast<std::uint8_t>(amount_count);item->money=amount;
                world.pool=&pool;scene.pool=&pool;accepted=actor.receive_money_item(*item,world,pool);require(pool.statistics().available==1 && item->money==amount && item->item.type_id==type_id && item->item.count==amount_count,"Money receipt did not release/preserve the item");events.push_back("F");
            }
            const auto context=" at money vector "+std::to_string(count);const std::array<std::uint32_t,5> values{result,actor.money(),amount,accepted,static_cast<std::uint32_t>(pool.statistics().available)};
            for(std::size_t i=0;i<values.size();++i) {std::uint32_t original=0;row>>original;require(values[i]==original,"Money state differs"+context+" field "+std::to_string(i));}
            require(op==2 || amount==initial_amount,"Money function changed its caller's amount");
            require(world.replica.money()==(op>=4?actor.money():current),"Money helper persisted state without its DB-agent update");
            std::size_t event_count=0;row>>event_count;require(events.size()==event_count,"Money event count differs"+context);for(const auto& event:events) {std::string original;row>>original;require(event==original,"Money event differs"+context+": "+event+" versus "+original);}
            require(static_cast<bool>(row),"Truncated money vector");std::string extra;require(!(row>>extra),"Trailing money vector data");++count;
        }
        require(count==1792,"Money oracle coverage changed");
        std::vector<std::string> events;Scene scene(events);World world(events);GroundItemPool pool;const MapGeometry geometry(160);ZoneItems zone(geometry,world);const ItemQuestLinks links;
        Actor actor(9001,{}, {},{},scene,{items,enchant,mobs});actor.account_id()=12345;actor.money()=0xfffffff0U;world.actor=&actor;world.pool=&pool;scene.pool=&pool;world.position={10,20,10};world.replica=MoneyReplica(12345,actor.money());
        auto* item=pool.allocate();item->item.type=26;item->item.count=1;item->money=100;item->position=world.position;const auto id=item->id;require(zone.enter(*item,1),"Cannot place concrete money pickup");events.clear();std::int32_t error=123;
        auto* picked=zone.pick(actor,id,error,true,links,world);require(picked==item && error==123 && zone.size()==0 && pool.statistics().available==0,"Concrete money pickup did not detach its allocation");
        const auto accepted=actor.receive_money_item(*picked,world,pool);require(accepted==15 && actor.money()==0xffffffffU && pool.statistics().available==1 && item->money==100,"Concrete money pickup did not clamp credit and return its allocation");
        require(events.size()==7 && events[0]=="P" && events[1]=="D" && events[2].starts_with("V0204") && events[3].starts_with("B0306") && events[4]=="S"+hex(money_received_packet(15)) && events[5]=="T" && events[6].starts_with("G0108"),"Concrete money pickup/credit route ordering changed");
        require(world.replica.money()==actor.money() && !world.replica.apply(money_database_packet(999,0)) && !world.replica.apply(net::Packet{3,6}) && world.replica.money()==0xffffffffU,"Money replica accepted another account or a truncated update");
        std::cout<<count<<" original money cases and concrete zone-to-credit/pool integration passed\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
