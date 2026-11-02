#include "eternity/game/actor.hpp"
#include "eternity/game/actor_load.hpp"
#include "eternity/game/inventory.hpp"
#include "eternity/game/item_pool.hpp"
#include "equipment_fixture.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>

using namespace eternity;using namespace eternity::game;
namespace {
void require(bool value,const std::string& message) {if(!value) throw std::runtime_error(message);}
std::string hex(const auto& data) {static constexpr char digits[]="0123456789abcdef";std::string result;for(auto byte:data) {const auto v=static_cast<std::uint8_t>(byte);result+=digits[v>>4];result+=digits[v&15];}return result;}
std::array<std::uint8_t,6> gems(const std::string& value) {require(value.size()==12,"Bad gems");std::array<std::uint8_t,6> result;for(std::size_t i=0;i<6;++i) result[i]=static_cast<std::uint8_t>(std::stoul(value.substr(i*2,2),nullptr,16));return result;}
std::uint64_t get(const net::Packet& packet,std::size_t offset,unsigned int bytes) {std::uint64_t value=0;for(unsigned int i=0;i<bytes;++i) value|=static_cast<std::uint64_t>(packet.at(offset+i))<<(8*i);return value;}
struct Trace {
    GroundItemPool& pool;std::vector<std::string> events;std::vector<std::uint32_t> pending;std::size_t available=0;
    void flush() {const auto next=pool.statistics().available;while(available<next) {require(!pending.empty(),"Unexpected freed inventory object");events.push_back("F"+std::to_string(pending.front()));pending.erase(pending.begin());++available;}}
    void emit(std::string value) {flush();events.push_back(std::move(value));}
};
struct Scene final:ActorScene {
    Trace& trace;std::uint32_t id;std::optional<bool> party;Scene(Trace& value,std::uint32_t identity):trace(value),id(identity) {}
    void send_self(const net::Packet& packet) noexcept override {trace.emit("S"+std::to_string(id)+":"+hex(packet));}
    void send_nearby(const net::Packet& packet) override {trace.emit("N"+hex(packet));}void send_cell_view(const net::Packet& packet) override {trace.emit("C"+hex(packet));}
    std::optional<bool> party_kind() const noexcept override {return party;}void send_party(const net::Packet& packet) noexcept override {trace.emit("P"+hex(packet));}
    void reset_damage() override {trace.emit("X");}std::optional<std::uint32_t> allocate_effect_id() noexcept override {std::terminate();}void release_effect_id(std::uint32_t) noexcept override {std::terminate();}
    std::uint32_t tick_ms() const noexcept override {std::terminate();}void unexpected() {throw std::runtime_error("Unexpected removal scene operation");}
    void stop_target_mob(std::uint32_t) override {unexpected();}void taunt(std::uint16_t,float) override {unexpected();}ZoneUsers* user_zone() override {unexpected();return nullptr;}UserShapeSocial user_shape_social() override {unexpected();return {};}
    void finish_periodic_damage(EffectStatus&,const SkillDefinition&,const EffectSource&,std::uint32_t) override {unexpected();}
};
struct World final:InventoryRemovalWorld {
    Trace& trace;Actor* peer=nullptr;bool available=false;explicit World(Trace& value):trace(value) {}
    void unexpected() {throw std::runtime_error("Unexpected removal world operation");}
    std::uint32_t lock_inventory_party(Actor&) override {unexpected();return 0;}void unlock_inventory_party(std::uint32_t) noexcept override {std::terminate();}
    std::optional<bool> inventory_party_kind(const Actor&) override {unexpected();return std::nullopt;}void send_inventory_party(Actor&,const net::Packet&) override {unexpected();}
    void send_inventory_database(const net::Packet& packet) override {trace.emit("B"+hex(packet));}void send_money_database(const net::Packet&) override {unexpected();}
    GameLogSubject money_log_subject(const Actor&) override {GameLogSubject result;result.account_id=12345;result.character_id=23456;result.experience=0xffff0000;result.account_name="removal-account";result.character_name="RemovalCharacter";result.level=1;result.map=7;result.position={10.25F,-1.5F,10.75F};return result;}
    std::chrono::sys_seconds money_local_calendar() override {trace.emit("T");return std::chrono::sys_days{std::chrono::year{2026}/11/2}+std::chrono::hours{13}+std::chrono::minutes{47}+std::chrono::seconds{59};}
    void send_money_log(const net::Packet& packet) override {trace.emit("G"+hex(packet));trace.pending.push_back(static_cast<std::uint32_t>(get(packet,74,8)-0x1122334455667700ULL+2));}
    std::string inventory_item_name(const ItemDefinition&) override {return "Removal item";}std::string inventory_drop_user_name(Actor&,std::uint32_t,bool) override {unexpected();return {};}
    bool return_inventory_item(Actor&,GroundItem&) override {unexpected();return false;}
    Actor* find_exchange_user(Actor&,std::uint32_t id) override {trace.emit("Q"+std::to_string(id));return available && id==9002?peer:nullptr;}
    void negative_craft_reduction(const PlayerItem&,std::int32_t value) override {trace.emit("D"+std::to_string(value));}
};
EffectStatus initial_status(bool shape,bool bound) {
    EffectStatus result;result.level=1;result.shape=static_cast<std::uint8_t>(shape);result.in_zone=bound;auto& s=result.skills.equipment;s.transformed=shape;s.base={Attributes{100000,100000,100000,100000,100000,100000},PlayerResources{100000,100000,100000}};
    s.attack.item_attack=100000;s.variance=100000;s.attack.physical_defense=100000;s.attack.magic_defense=100000;s.range=100000;s.attack_speed=100000;s.move_speed=100000;s.damage_reduction=100000;s.attack_attribute=100000;s.defense_attribute=100000;s.standing_recovery.fill(100000);s.sitting_recovery.fill(100000);s.state_blocks.fill(100000);s.shield_value=9;s.weapon_speed.fill(3);s.weapon_damage.fill(7);return result;
}
}
int main(int argc,char** argv) {
    try {
        require(argc==2,"Expected item removal oracle");std::ifstream input(argv[1]);require(input.good(),"Cannot open removal oracle");const auto definitions=ItemDefinitions::load(item_definitions());EnchantValues enchant;for(std::size_t i=0;i<21;++i) enchant.additions[i]={static_cast<std::uint16_t>(i*7),static_cast<std::uint16_t>(i*11)};const MobAttackTemplates mobs;std::string line;unsigned int checked=0;
        while(std::getline(input,line)) {
            if(line.empty() || line[0]=='#') continue;std::istringstream row(line);unsigned int op=0,bag=0,slot=0;std::int32_t bags=0,query=0,query_id=0;int group=-1;bool bound=false,shape=false,initializing=false,trade=false,duel=false,available=false;std::size_t count=0;
            row>>op>>bag>>slot>>bags>>query>>query_id>>bound>>group>>shape>>initializing>>trade>>duel>>available>>count;
            GroundItemPool pool;Trace trace{pool};Scene scene(trace,9001),other(trace,9002);if(group>=0) scene.party=group!=0;World world(trace);world.available=available;
            Actor actor(9001,initial_status(shape,bound),{100000,100100,100200},{},scene,{definitions,enchant,mobs});Actor peer(9002,{}, {},{},other,{definitions,enchant,mobs});world.peer=&peer;actor.account_id()=12345;actor.status().skills.max_bags=bags;actor.initializing(initializing);
            for(std::size_t i=0;i<count;++i) {
                unsigned int b=0,s=0,type=0,amount=0,quality=0,tier=0,saved=0,make=0;std::string gem;PlayerItem item;
                row>>b>>s>>type>>amount>>quality>>tier>>saved>>item.made_at>>make>>gem>>item.craft>>item.uid;item.bag=static_cast<std::uint8_t>(b);item.slot=static_cast<std::uint16_t>(s);item.type=static_cast<std::uint8_t>(type);item.type_id=1;item.count=static_cast<std::uint8_t>(amount);item.quality=static_cast<std::uint16_t>(quality);item.gems=gems(gem);if(make) item.make_type.assign(1,static_cast<char>(make));require(actor.load_inventory_item(item,pool),"Cannot load pooled inventory fixture");
                if(b==0) {actor.status().skills.equipment.quality[s]=static_cast<std::uint8_t>(tier);actor.equipment_database_quality()[s]=static_cast<std::uint16_t>(saved);}
            }
            actor.exchange().mode=trade?3:0;actor.exchange().peer=&peer;actor.duel_exchange().mode=duel?3:0;actor.relations().duel_state=duel?5:0;actor.relations().duel_opponent=9002;
            for(auto* owner:{&actor,&peer}) {owner->exchange().ready=1;owner->duel_exchange().ready=2;}actor.exchange().items[0]={static_cast<std::uint8_t>(bag),static_cast<std::uint8_t>(slot),1};actor.duel_exchange().items[0]=actor.exchange().items[0];trace.events.clear();
            if(op==3) actor.send_equipment(static_cast<std::uint8_t>(slot));else if(op==2) require(actor.delete_items(query,query_id,world),"ItemDelete stopped returning true");else require(actor.remove_item(static_cast<std::uint8_t>(bag),static_cast<std::uint16_t>(slot),world,op==0)==(count!=0),"Native removal presence result differs");trace.flush();
            const auto& status=actor.status().skills.equipment;const auto& a=status.base.attributes;const auto& m=status.base.maximum;
            std::vector<std::int64_t> values{a.strength,a.dexterity,a.recovery,a.intelligence,a.wisdom,a.luck,m.hp,m.mp,m.sp,status.attack.item_attack,status.variance,status.attack.physical_defense,status.attack.magic_defense,status.range,status.attack_speed,status.move_speed,status.damage_reduction,status.attack_attribute,status.defense_attribute};values.insert(values.end(),status.standing_recovery.begin(),status.standing_recovery.end());values.insert(values.end(),status.sitting_recovery.begin(),status.sitting_recovery.end());values.insert(values.end(),status.state_blocks.begin(),status.state_blocks.end());values.push_back(actor.resources().hp);values.push_back(actor.resources().mp);values.push_back(actor.resources().sp);values.insert(values.end(),status.quality.begin(),status.quality.end());values.insert(values.end(),actor.equipment_database_quality().begin(),actor.equipment_database_quality().end());
            const auto context=" at item removal vector "+std::to_string(checked);
            for(std::size_t i=0;i<values.size();++i) {std::int64_t original=0;row>>original;require(values[i]==original,"Removal field "+std::to_string(i)+" differs"+context+": "+std::to_string(values[i])+" != "+std::to_string(original));}
            std::size_t remaining=0;row>>remaining;require(actor.inventory().size()==remaining,"Removal inventory size differs"+context);
            for(const auto& item:actor.inventory()) for(auto field:std::array<std::uint64_t,5>{item.bag,item.slot,item.uid,item.count,item.quality}) {std::uint64_t original=0;row>>original;require(field==original,"Surviving inventory differs"+context);}
            std::size_t freed=0;row>>freed;require(freed==pool.statistics().available,"Pool release differs"+context);
            for(auto field:std::array<unsigned int,4>{actor.exchange().ready,actor.duel_exchange().ready,peer.exchange().ready,peer.duel_exchange().ready}) {unsigned int original=0;row>>original;require(field==original,"Offer readiness differs"+context);}
            for(const auto* offer:{&actor.exchange(),&actor.duel_exchange()}) {net::Packet bytes;for(const auto& item:offer->items) {bytes.push_back(item.bag);bytes.push_back(item.slot);bytes.push_back(item.count);}std::string original;row>>original;require(hex(bytes)==original,"Offer reference differs"+context);}
            std::size_t events=0;row>>events;require(trace.events.size()==events,"Removal event count differs"+context+": "+std::to_string(trace.events.size())+" != "+std::to_string(events));for(const auto& event:trace.events) {std::string original;row>>original;require(event==original,"Removal event differs"+context+": "+event+" != "+original);}require(static_cast<bool>(row),"Truncated removal vector");std::string extra;require(!(row>>extra),"Trailing removal data");++checked;
        }
        require(checked==1408,"Removal coverage changed");
        // Real actor preparation materializes resolved SQL inventory. Allocation
        // failure skips that row, and successful ownership lasts until teardown.
        const auto statuses=StatusRules::load({{"Level","Job","HP","MP","SP"},{{"1","0","100","200","300"}}});const SkillDefinitions skills;const auto quests=QuestDefinitions::decode(net::Packet{0,0,0,0});const ActorLoadRules rules{statuses,definitions,enchant,skills,quests,mobs};PlayerSnapshot player;player.core.character.id=9001;player.core.character.level=1;player.core.character.map=1;player.core.character.hp=100;player.core.character.mp=200;player.core.character.sp=300;PlayerItem loaded;loaded.bag=1;loaded.type=5;loaded.type_id=1;loaded.count=1;loaded.quality=1000;loaded.craft="short";player.items.push_back(loaded);loaded.bag=0;loaded.slot=23;player.items.push_back(loaded);
        for(bool fail:{false,true}) {GroundItemPool pool(1,[&]()->std::unique_ptr<GroundItem>{return fail?nullptr:std::make_unique<GroundItem>();});Trace trace{pool};Scene scene(trace,9001);{ActorLoadOptions options;options.inventory_pool=&pool;auto result=load_actor(player,rules,options,scene);require(result.actor && result.actor->inventory().size()==(fail?0U:2U),"Actor pooled loading lost allocation failure semantics");if(!fail) require(result.actor->inventory()[0].craft==std::string(20,'0'),"Pooled SQL row did not normalize craft");}require(pool.statistics().available==(fail?0U:2U),"Actor pool ownership leaked at teardown");}
        std::cout<<checked<<" original item removal/delete cases and pooled actor loading passed\n";
    }catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
