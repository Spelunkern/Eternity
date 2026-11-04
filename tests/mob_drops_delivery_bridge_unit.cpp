#include "eternity/game/mob_drops_delivery.hpp"
#include "eternity/game/actor.hpp"
#include <algorithm>
#include <bit>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace eternity;using namespace eternity::game;
namespace {
void check(bool value,const std::string& why) {if(!value) throw std::runtime_error(why);}
std::string payload(const GroundItem& item) {
    std::string bytes;const auto add=[&](std::uint64_t value,unsigned count) {for(unsigned i=0;i<count;++i) bytes+=static_cast<char>(value>>(i*8));};
    add(item.item.uid,8);add(item.item.type,1);add(item.item.type_id,1);add(item.item.count,1);add(item.item.quality,2);for(auto v:item.item.gems) add(v,1);for(auto v:item.craft_storage) add(v,1);add(item.item.made_at,4);add(item.item.make_type.empty()?0:static_cast<unsigned char>(item.item.make_type[0]),1);
    add(item.user_id,4);add(item.party_id,4);add(item.drop_type,1);add(item.drop_id,4);add(item.money,4);for(auto v:item.craft_additions) add(v,2);add(std::bit_cast<std::uint32_t>(item.position.x),4);add(std::bit_cast<std::uint32_t>(item.position.y),4);add(std::bit_cast<std::uint32_t>(item.position.z),4);add(item.id,4);add(item.definition?1:0,1);
    std::string result;constexpr char hex[]="0123456789abcdef";for(unsigned char value:bytes) {result+=hex[value>>4];result+=hex[value&15];}return result;
}
struct Scene:ActorScene {
    std::vector<net::Packet> packets;
    void send_self(const net::Packet& packet) noexcept override {packets.push_back(packet);}
    [[noreturn]] static void unexpected() {throw std::runtime_error("Unexpected bridge actor scene call");}
    void send_nearby(const net::Packet&) override {unexpected();}void send_cell_view(const net::Packet&) override {unexpected();}
    std::optional<bool> party_kind() const noexcept override {std::terminate();}void send_party(const net::Packet&) noexcept override {std::terminate();}
    void reset_damage() override {unexpected();}std::optional<std::uint32_t> allocate_effect_id() noexcept override {std::terminate();}void release_effect_id(std::uint32_t) noexcept override {std::terminate();}
    std::uint32_t tick_ms() const noexcept override {std::terminate();}void stop_target_mob(std::uint32_t) override {unexpected();}void taunt(std::uint16_t,float) override {unexpected();}
    ZoneUsers* user_zone() override {unexpected();}UserShapeSocial user_shape_social() override {unexpected();}
    void finish_periodic_damage(EffectStatus&,const SkillDefinition&,const EffectSource&,std::uint32_t) override {unexpected();}
};
struct Services:MobDropDeliveryServices,InventoryWorld,ZoneItemServices {
    std::vector<std::string>& flow;GroundItemPool& pool;ItemDefinition& canonical;std::array<std::unique_ptr<Actor>,5>* actors=nullptr;ZoneItems* zone=nullptr;
    std::uint64_t raw=0;unsigned grants=0,logs=0,released=0;std::vector<net::Packet> database,views;std::function<void(GroundItem&)> on_log;
    Services(std::vector<std::string>& f,GroundItemPool& p,ItemDefinition& d):flow(f),pool(p),canonical(d) {}
    static std::uint32_t opcode(const net::Packet& p) {check(p.size()>=2,"Truncated bridge packet");return p[0]|(std::uint32_t(p[1])<<8);}
    const ItemDefinition& definition(const MobLootItem&) override {return canonical;}
    std::uint32_t make_time() override {flow.push_back("T");return 0x12345678;}
    std::uint64_t raw_unique_id() override {flow.push_back("U");return raw;}
    MobDropInventoryTarget recipient(MobLootUser& user,GroundItem& item) override {check(actors && user.id>=100 && user.id<105,"Bad real recipient");flow.push_back("G"+std::to_string(user.id)+":"+payload(item));++grants;return {*(*actors)[user.id-100],*this};}
    void sequence_log(GroundItem& item,MobLootUser*) override {check(item.item.make_type=="V","Sequence mark missing before log");flow.push_back("V:"+payload(item));}
    void log_item(GroundItem& item,MobLootUser* user) override {flow.push_back("L"+std::to_string(user?user->id:0)+":"+payload(item));if(on_log) on_log(item);}
    std::uint32_t lock_inventory_party(Actor&) override {return 0;}
    void unlock_inventory_party(std::uint32_t) noexcept override {std::terminate();}
    std::optional<bool> inventory_party_kind(const Actor&) override {return {};}
    void send_inventory_party(Actor&,const net::Packet&) override {throw std::runtime_error("Unexpected unregistered actor party notice");}
    void send_inventory_database(const net::Packet& p) override {check(opcode(p)==0x701 || opcode(p)==0x707,"Bridge did not invoke real inventory update");database.push_back(p);}
    void send_money_database(const net::Packet&) override {throw std::runtime_error("Unexpected item money grant");}
    GameLogSubject money_log_subject(const Actor& actor) override {GameLogSubject subject;subject.account_id=actor.account_id();subject.character_id=actor.id();subject.account_name="Bridge";subject.character_name="Recipient";return subject;}
    std::chrono::sys_seconds money_local_calendar() override {return std::chrono::sys_days{std::chrono::year{2026}/11/3};}
    void send_money_log(const net::Packet& packet) override {check(opcode(packet)==0x401 && packet.size()==151,"Missing real inventory loot log");++logs;}
    std::string inventory_item_name(const ItemDefinition&) override {return "Bridge item";}
    std::string inventory_drop_user_name(Actor&,std::uint32_t,bool) override {throw std::runtime_error("Unexpected user drop lookup");}
    bool return_inventory_item(Actor&,GroundItem& item) override {return zone->enter(item,1);}
    std::uint32_t tick_ms() override {return 1000;}
    void send_cell_view(CellCoordinates,const net::Packet& packet) override {
        views.push_back(packet);if(opcode(packet)!=0x401) return;const auto id=std::uint32_t(packet[2])|(std::uint32_t(packet[3])<<8)|(std::uint32_t(packet[4])<<16)|(std::uint32_t(packet[5])<<24);
        auto* item=zone->find(id);check(item && item->zone==zone,"Appearance before actual zone membership");flow.push_back("E:"+payload(*item));
    }
    void release(GroundItem& item) override {++released;pool.release(item);}
};
}
int main(int argc,char** argv) {try {
    check(argc==2,"Expected full bridge oracle");std::ifstream input(argv[1]);check(input.good(),"Cannot read bridge oracle");unsigned cases=0,total_grants=0,total_ground=0;
    const ItemDefinitions definitions;const EnchantValues enchant;const MobAttackTemplates mobs;const MapGeometry geometry(81);
    for(std::string line;std::getline(input,line);) {
        if(line.empty() || line[0]=='#') continue;std::istringstream row(line);unsigned scenario,kind,present,dex,logged,sequence,allocation,have,server,wis,quality,server_id,nulls;int mode,cursor,real;std::uint32_t seed,identity,final_cursor,final_seed;std::uint64_t raw;
        row>>scenario>>seed>>mode>>kind>>cursor>>present>>dex>>real>>logged>>sequence>>allocation>>have>>server>>wis>>quality>>server_id>>raw>>nulls>>identity;
        std::array<MobLootUser,5> users{};MobLootParty party;party.id=77;party.mode=mode;party.type=static_cast<std::uint8_t>(kind);party.leader=2;party.cursor=cursor;
        for(unsigned i=0;i<5;++i) {unsigned instance;std::uint32_t x,z;auto& user=users[i];user.id=100+i;row>>user.state>>user.map>>instance>>user.dead>>x>>z;user.instance=static_cast<std::uint16_t>(instance);user.x=std::bit_cast<float>(x);user.z=std::bit_cast<float>(z);party.members.push_back(nulls&(1U<<i)?nullptr:&user);}
        std::string expected;row>>expected>>final_cursor>>final_seed;check(!row.fail(),"Malformed full bridge scenario");
        ItemDefinition canonical;canonical.id=1002;canonical.type=1;canonical.type_id=2;canonical.stack_count=1;canonical.quality[0]=static_cast<std::uint16_t>(quality);canonical.server=static_cast<std::uint8_t>(server);canonical.required_wisdom=static_cast<std::uint16_t>(wis);canonical.real_type=static_cast<std::uint8_t>(real);
        MobLootItem projected{1002,1,2,false,static_cast<std::uint16_t>(quality),static_cast<std::uint16_t>(dex),real};std::vector<std::string> flow;
        GroundItemPool pool(0x1233,[&]() -> std::unique_ptr<GroundItem> {flow.push_back("A");return allocation?std::make_unique<GroundItem>():nullptr;});Scene scene;Services services(flow,pool,canonical);services.raw=raw;ZoneItems zone(geometry,services);services.zone=&zone;
        std::array<std::unique_ptr<Actor>,5> actors;for(unsigned i=0;i<5;++i) {actors[i]=std::make_unique<Actor>(100+i,EffectStatus{},PlayerResources{},std::vector<PersistedEffect>{},scene,ActorEquipmentRules{definitions,enchant,mobs});actors[i]->account_id()=10100+i;actors[i]->status().skills.max_bags=1;}services.actors=&actors;
        MobDropDeliveryContext context{{17,static_cast<std::uint8_t>(server_id),80,80},identity};MicrosoftRandom random(seed);
        const auto result=deliver_mob_drop(have?&projected:nullptr,&users[0],present?&party:nullptr,sequence,logged,context,pool,zone,random,services);
        std::string actual;for(const auto& event:flow) {if(!actual.empty()) actual+=',';actual+=event;}
        check((actual.empty()?"-":actual)==expected,"Full original bridge trace differs: "+std::to_string(scenario));check(static_cast<std::uint32_t>(party.cursor)==final_cursor && random.state()==final_seed,"Full original bridge cursor/RNG differs");
        if(!have) check(result.destination==MobDropDestination::none,"Missing definition allocated a drop");
        else if(!allocation) check(result.destination==MobDropDestination::allocation_failed,"Allocation failure was hidden");
        else if(services.grants) {check(result.destination==MobDropDestination::inventory && result.receipt==InventoryReceipt::stored && !result.item,"Real inventory ownership outcome differs");check(zone.size()==0 && services.grants==1,"Granted drop also entered zone");}
        else {check(result.destination==MobDropDestination::ground && result.item && result.item->zone==&zone && zone.size()==1,"Drop was not actually published");++total_ground;}
        std::size_t inventory=0;for(const auto& actor:actors) inventory+=actor->inventory().size();check(inventory==services.grants && services.database.size()==services.grants && scene.packets.size()==services.grants && services.logs==services.grants,"Real Actor admission/DB/client/log effects missing");
        for(const auto& p:scene.packets) check(Services::opcode(p)==0x205,"Wrong real receipt packet");total_grants+=services.grants;zone.update(61001);check(zone.size()==0,"Bridge ground drop did not expire");++cases;
    }
    // Invalid geometry is explicit retained ownership, not a successful no-op.
    std::vector<std::string> flow;ItemDefinition canonical;canonical.id=1002;canonical.type=1;canonical.type_id=2;GroundItemPool pool;Services services(flow,pool,canonical);ZoneItems zone(geometry,services);services.zone=&zone;MicrosoftRandom random(1);
    auto money=place_mob_drop_money({17,0,80,80},42,12345,pool,zone,random);check(money.destination==MobDropDestination::ground && money.item && money.item->money==12345 && money.item->user_id==42,"Real money publication failed");zone.update(61001);
    auto outside=place_mob_drop_money({17,0,100000,100000},42,7,pool,zone,random);check(outside.destination==MobDropDestination::unlinked && outside.item && !outside.item->zone,"Rejected geometry lost explicit allocation");pool.release(*outside.item);
    // A nested delivery during a required log retains two independent handles.
    MobDropDeliveryContext context{{17,0,80,80},7};MobLootItem projected{1002,1,2,false,0,1,0};bool nested=false;GroundItem* nested_item=nullptr;
    services.on_log=[&](GroundItem&) {if(nested) return;nested=true;services.raw+=0x10000;const auto result=deliver_mob_drop(&projected,nullptr,nullptr,false,false,context,pool,zone,random,services);check(result.destination==MobDropDestination::ground,"Nested drop was not published");nested_item=result.item;};
    const auto outer=deliver_mob_drop(&projected,nullptr,nullptr,false,true,context,pool,zone,random,services);
    check(outer.destination==MobDropDestination::ground && outer.item && nested_item && outer.item!=nested_item && zone.size()==2,"Nested delivery overwrote outer handle");zone.update(61001);services.on_log={};
    // A grant can recycle the incoming allocation before the caller logs it.
    // The result must not expose that released handle as caller-owned storage.
    canonical.stack_count=10;Scene scene;std::array<std::unique_ptr<Actor>,5> actors;actors[0]=std::make_unique<Actor>(100,EffectStatus{},PlayerResources{},std::vector<PersistedEffect>{},scene,ActorEquipmentRules{definitions,enchant,mobs});actors[0]->account_id()=10100;actors[0]->status().skills.max_bags=1;services.actors=&actors;
    MobLootUser member;member.id=100;member.map=7;member.x=80;member.z=80;MobLootParty party;party.id=77;party.mode=2;party.members={&member};
    services.raw+=0x10000;const auto stored=deliver_mob_drop(&projected,&member,&party,false,true,context,pool,zone,random,services);
    services.raw+=0x10000;const auto merged=deliver_mob_drop(&projected,&member,&party,false,true,context,pool,zone,random,services);
    check(stored.receipt==InventoryReceipt::stored && merged.receipt==InventoryReceipt::merged && !stored.item && !merged.item && actors[0]->inventory().size()==1 && actors[0]->inventory()[0].count==2,"Real merged receipt retained a caller-owned pointer");
    check(Services::opcode(services.database.back())==0x707 && services.database.back().size()==9 && services.database.back()[8]==2,"Real merged stack did not publish original 0707 count update");
    actors[0]->status().skills.max_bags=0;services.raw+=0x10000;
    const auto returned=deliver_mob_drop(&projected,&member,&party,false,true,context,pool,zone,random,services);
    check(returned.destination==MobDropDestination::inventory && returned.receipt==InventoryReceipt::returned && !returned.item && zone.size()==1 && services.views.back()[6]==1,"Real full-inventory return did not transfer to zone owner");zone.update(61001);
    check(cases==512 && total_grants>0 && total_ground>0,"Incomplete bridge coverage");std::cout<<cases<<" complete original drops through real pool/craft/range/Actor/ZoneItems; "<<total_grants<<" inventory admissions and "<<total_ground<<" ground publications matched\n";
} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}}
