#include "eternity/game/quest_requests_actor.hpp"
#include "eternity/game/inventory_replica.hpp"
#include "eternity/game/quest_replica.hpp"
#include "eternity/game/item_pool.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace eternity::game;
namespace {
void check(bool ok,const char* text) {if(!ok) throw std::runtime_error(text);}
std::vector<std::uint8_t> unhex(const std::string& s) {std::vector<std::uint8_t> p;if(s=="-") return p;for(std::size_t i=0;i<s.size();i+=2) p.push_back(static_cast<std::uint8_t>(std::stoul(s.substr(i,2),nullptr,16)));return p;}
void append(std::vector<std::uint8_t>& p,std::uint32_t n) {for(unsigned i=0;i<4;++i) p.push_back(static_cast<std::uint8_t>(n>>(8*i)));}
struct QuestPool final:QuestRuntimePoolServices {std::unique_ptr<RestoredQuest> construct() override {return std::make_unique<RestoredQuest>();}};
struct Services final:ActorScene,QuestRequestsActorPorts,QuestRewardWorld {
    bool legacy=false,reentry=false,found=true,break_identity=false;unsigned depth=0,bags=0;std::uint32_t now=0;
    std::vector<std::uint8_t> events;std::vector<std::uint64_t> released;
    GroundItemPool item_pool;std::array<ItemDefinition,218> item_defs{};
    ItemDefinitions definitions;EnchantValues enchant;MobAttackTemplates mobs;
    Actor actor{9001,{}, {},{},*this,{definitions,enchant,mobs}};
    InventoryReplicaPool inventory_cache_pool;InventoryReplica inventory_cache;
    QuestReplicaPool quest_cache_pool;QuestReplica quest_cache{7,quest_cache_pool};
    QuestPool allocation;QuestRuntimePool quest_pool{allocation};QuestRuntime runtime{7,quest_pool};
    QuestRequestsActorInventory inventory{actor,*this};QuestRequestsActorWorld* world=nullptr;
    RestoredQuest* target=nullptr;RestoredQuest* other=nullptr;QuestRequestNpc npc;
    explicit Services(bool old=false):legacy(old),inventory_cache(old?0x12345678:7,17,inventory_cache_pool) {actor.account_id()=old?0x12345678:7;actor.money()=17;}
    void event(char c,std::initializer_list<std::uint32_t> values={}) {events.push_back(static_cast<std::uint8_t>(c));for(auto v:values) append(events,v);}
    void packet(char c,const eternity::net::Packet& p) {events.push_back(static_cast<std::uint8_t>(c));if(!legacy) append(events,static_cast<std::uint32_t>(p.size()));events.insert(events.end(),p.begin(),p.end());}
    void load(std::span<const std::uint8_t> bytes,unsigned unlocked) {
        bags=unlocked;actor.status().skills.max_bags=static_cast<decltype(actor.status().skills.max_bags)>(bags);
        check(bytes.size()==bags*24*5,"bad Actor inventory fixture");
        auto claim=[&](unsigned index,unsigned bag,unsigned slot,std::uint8_t type,std::uint8_t id,std::uint8_t count,std::uint8_t real,std::uint64_t uid) {
            auto& definition=item_defs[index];definition.id=100+index;definition.type=type;definition.type_id=id;definition.real_type=real;
            auto* item=item_pool.allocate();check(item!=nullptr,"item pool allocation failed");item->definition=&definition;
            item->item.uid=uid;item->item.type=type;item->item.type_id=id;item->item.count=count;
            check(actor.claim_inventory_item(static_cast<std::uint8_t>(bag),static_cast<std::uint16_t>(slot),*item,item_pool),"raw fixture claim failed");
            check(inventory_cache.load(item->item),"cannot seed inventory replica");
        };
        for(unsigned i=0;i<bags*24;++i) if(bytes[i*5]) claim(i,i/24+1,i%24,bytes[i*5+1],bytes[i*5+2],bytes[i*5+3],bytes[i*5+4],1000+i);
        claim(216,0,0,1,1,13,0,999);
        if(bags<5) claim(217,bags+1,0,1,1,17,0,998);
    }
    void verify_mirror() {
        for(const auto& row:actor.inventory()) {const auto held=actor.retained_inventory_item(row.bag,row.slot);check(held && held->pool==&item_pool && held->item->item.uid==row.uid && held->item->item.count==row.count,"Actor mirror/pointer mismatch");}
    }
    void send_database(Actor& a,const eternity::net::Packet& p) override {
        check(&a==&actor,"wrong Actor transport");verify_mirror();packet('B',p);
        if(p[0]==2 && p[1]==7) {
            check(inventory_cache.apply(p)==InventoryReplicaResult::applied,"inventory DB packet rejected");
            if(break_identity) {const auto held=actor.retained_inventory_item(p[6],p[7]);check(held && actor.release_inventory_item(p[6],p[7],held->item),"test release failed");}
        } else if(p[0]==3 && p[1]==6) check(inventory_cache.apply_money(p),"reward money DB packet rejected");
        else if(p[1]==10) check(quest_cache.apply(p)==InventoryReplicaResult::applied,"quest DB packet rejected");
        else throw std::runtime_error("unexpected Actor DB packet");
        if(reentry) {check(runtime.find_active(2)==other,"shared cursor reentry failed");event('J',{2});}
    }
    void send_database(const eternity::net::Packet& p) override {send_database(actor,p);}
    void send_self(const eternity::net::Packet& p) noexcept override {
        try {
            verify_mirror();packet('C',p);
            if(p.size()==7 && p[0]==6 && p[1]==2) {
                const auto held=actor.retained_inventory_item(p[2],p[3]);check(held.has_value(),"client sees missing retained item");
                const auto saved=inventory_cache.inventory();const auto at=std::find_if(saved.begin(),saved.end(),[&](const auto& q){return q.item.bag==p[2] && q.item.slot==p[3];});
                check(p[6]?at!=saved.end() && at->item.count==p[6]:at==saved.end(),"client preceded DB mutation");
                if(!p[6]) {released.push_back(held->item->item.uid);if(legacy) events.insert(events.end(),{'F',p[2],p[3],held->item->item.count});else event('F',{p[2],p[3],held->item->item.count});}
            }
        } catch(...) {std::terminate();}
    }
    void lock_quests(Actor& a) override {check(&a==&actor,"wrong Actor lock");++depth;}
    void unlock_quests(Actor&) noexcept override {--depth;}
    std::uint32_t tick_ms() override {event('T');return now;}
    std::optional<QuestRequestNpc> find_npc(Actor&,std::uint32_t id) override {event('N',{id});return found?std::optional{npc}:std::nullopt;}
    void fill_game_log_main(Actor&,std::span<std::uint8_t> p) override {event('M');std::fill(p.begin()+2,p.end(),0);}
    void send_game_log(Actor&,const eternity::net::Packet& p) override {packet('G',p);}
    QuestRewardWorld& reward_world(Actor&) override {return *this;}
    void add_experience(std::uint32_t source,std::int32_t amount,bool bypass) override {event('X',{source,static_cast<std::uint32_t>(amount),bypass?1U:0U});}
    void fill_game_log_main(std::span<std::uint8_t> p) override {fill_game_log_main(actor,p);}
    std::optional<QuestRewardItemReceipt> receive_item(std::uint8_t type,std::uint8_t id,std::uint8_t count) override {event('I',{type,id,count});return {};}
    void remove_quest(RestoredQuest& q,bool success) override {check(world!=nullptr,"reward lacks live quest world");world->remove_quest(q,success);}
    void send_game_log(const eternity::net::Packet& p) override {send_game_log(actor,p);}
    [[noreturn]] static void unexpected() {throw std::runtime_error("unexpected quest Actor scene side effect");}
    void send_nearby(const eternity::net::Packet&) override {unexpected();}
    void send_cell_view(const eternity::net::Packet&) override {unexpected();}
    std::optional<bool> party_kind() const noexcept override {std::terminate();}
    void send_party(const eternity::net::Packet&) noexcept override {std::terminate();}
    void reset_damage() override {unexpected();}
    std::optional<std::uint32_t> allocate_effect_id() noexcept override {std::terminate();}
    void release_effect_id(std::uint32_t) noexcept override {std::terminate();}
    std::uint32_t tick_ms() const noexcept override {return now;}
    void stop_target_mob(std::uint32_t) override {unexpected();}
    void taunt(std::uint16_t,float) override {unexpected();}
    ZoneUsers* user_zone() override {unexpected();}
    UserShapeSocial user_shape_social() override {unexpected();}
    void finish_periodic_damage(EffectStatus&,const SkillDefinition&,const EffectSource&,std::uint32_t) override {unexpected();}
    std::vector<std::uint8_t> inventory_state() {
        std::vector<std::uint8_t> r;
        for(unsigned b=1;b<=bags;++b) for(unsigned s=0;s<24;++s) {
            const auto held=actor.retained_inventory_item(static_cast<std::uint8_t>(b),static_cast<std::uint16_t>(s));
            if(held) {const auto& item=*held->item;r.insert(r.end(),{1,item.item.type,item.item.type_id,item.item.count,item.definition->real_type});}else r.insert(r.end(),5,0);
        }
        return r;
    }
    std::string sequence(std::span<RestoredQuest* const> values) {std::string r;for(auto* q:values) {if(!r.empty()) r+=',';r+=q==target?'1':'2';}return r.empty()?"-":r;}
    void verify() {
        verify_mirror();const auto saved=inventory_cache.inventory();check(saved.size()==actor.inventory().size(),"Actor/cache size mismatch");
        for(const auto& row:actor.inventory()) {
            const auto at=std::find_if(saved.begin(),saved.end(),[&](const auto& q){return q.item.uid==row.uid;});check(at!=saved.end() && at->item.count==row.count,"Actor/cache count mismatch");
            if(row.uid==999 || row.uid==998) check(row.count==(row.uid==999?13:17),"equipment/locked bag changed");
        }
        check(inventory_cache.deleted_inventory()==released,"pool/DB deletion order mismatch");check(item_pool.statistics().available==released.size(),"pool direct release count mismatch");check(depth==0,"quest lock leaked");
    }
};
struct Flow final:QuestRequestWorld {
    Services& services;RestoredQuest quest;bool active=false;
    explicit Flow(Services& s):services(s) {}
    const std::array<std::uint8_t,464>* definition(std::uint16_t) override {return &quest.definition->record();}
    RestoredQuest* find_quest(std::uint16_t id) override {services.event('A',{id});return active?&quest:nullptr;}
    std::optional<QuestRequestNpc> find_npc(std::uint32_t id) override {return services.find_npc(services.actor,id);}
    QuestRequestInventory inventory() override {return services.inventory.refresh();}
    void send_database(const eternity::net::Packet& p) override {services.inventory.send_database(p);}
    void send_self(const eternity::net::Packet& p) override {services.inventory.send_self(p);}
    void release_item(std::uint8_t b,std::uint8_t s) override {services.inventory.release_item(b,s);}
    void complete_quest(RestoredQuest& q,std::uint32_t npc,bool success) override {services.event('E',{npc,success?1U:0U});services.events.insert(services.events.end(),q.packet.counters.begin(),q.packet.counters.end());}
};
ItemQuestLinks item_links() {std::vector<std::uint8_t> p(65536*8,0);const auto offset=(256+2)*8;p[offset+4]=3;p.insert(p.begin()+offset+8,{1,0,1,0,2,0});return ItemQuestLinks::decode(p);}
}
int main(int argc,char** argv) {try {
    check(argc==3,"two Actor fixtures required");unsigned flows=0,chains=0,releases=0;
    {std::ifstream input(argv[1]);check(input.good(),"cannot read Actor flow fixture");for(std::string line;std::getline(input,line);) {
        if(line.empty() || line[0]=='#') continue;
        std::istringstream r(line);unsigned scenario,id,npc,active,found,nt,ni,bags;std::string block,inventory,counters,final,final_counters,events;
        r>>scenario>>id>>npc>>active>>found>>nt>>ni>>bags>>block>>inventory>>counters>>final>>final_counters>>events;check(!r.fail(),"malformed Actor flow");
        const auto definitions=QuestDefinitions::decode(unhex(block));Services services(true);services.found=found!=0;services.npc={static_cast<std::int32_t>(nt),static_cast<std::int32_t>(ni)};services.load(unhex(inventory),bags);
        Flow world(services);world.active=active!=0;world.quest.definition=definitions.find(1);const auto c=unhex(counters);std::copy(c.begin(),c.end(),world.quest.packet.counters.begin());
        quest_end_request(static_cast<std::uint16_t>(id),npc,1,world);
        check(services.events==unhex(events) && services.inventory_state()==unhex(final) && std::vector<std::uint8_t>(world.quest.packet.counters.begin(),world.quest.packet.counters.end())==unhex(final_counters),"original Actor request flow differs");
        services.verify();releases+=static_cast<unsigned>(services.released.size());++flows;
    }}
    const auto links=item_links();
    {std::ifstream input(argv[2]);check(input.good(),"cannot read Actor chain fixture");for(std::string line;std::getline(input,line);) {
        if(line.empty() || line[0]=='#') continue;
        std::istringstream r(line);char kind;unsigned scenario,now,context,selection,found,bags,start,pending,money,success,cursor;std::string block,counters,inventory,events,final_counters,active,finished,free,final_inventory;
        r>>kind>>scenario>>now>>context>>selection>>found>>bags>>block>>start>>pending>>counters>>inventory>>events>>money>>final_counters>>success>>active>>finished>>free>>cursor>>final_inventory;check(!r.fail(),"malformed Actor chain");
        const auto definitions=QuestDefinitions::decode(unhex(block));Services s;s.now=now;s.reentry=scenario%19==0;s.found=found!=0;s.npc={definitions.find(1)->end_npc_type(),12};s.load(unhex(inventory),bags);
        RestoredQuest seed;seed.packet.id=1;seed.definition=definitions.find(1);seed.start_tick=start;seed.packet.end_tick=now;seed.success=static_cast<std::uint8_t>(pending);const auto c=unhex(counters);std::copy(c.begin(),c.end(),seed.packet.counters.begin());
        s.target=s.runtime.load_active(seed);seed={};seed.packet.id=2;seed.definition=definitions.find(2);s.other=s.runtime.load_active(seed);check(s.target && s.other,"quest owner allocation failed");
        check(s.quest_cache.load({1,0,{}}) && s.quest_cache.load({2,0,{}}),"quest cache load failed");QuestRequestsActorWorld world(s.actor,s.runtime,definitions,s);s.world=&world;
        if(kind=='R') quest_end_request(1,context,2,world);
        else if(kind=='S') quest_requests_complete_selection(1,static_cast<std::uint8_t>(selection),world);
        else if(kind=='F') world.end_quest(*s.target,context,false);
        else if(kind=='T') quest_requests_update_timers(now,world);
        else if(kind=='P') quest_requests_progress_pick_item(7,1,2,links,world);
        else if(kind=='K') quest_requests_progress_kill_pc(7,s.runtime.player_kill_quests(),world);
        else throw std::runtime_error("unknown Actor chain entry");
        const auto actual_cursor=s.runtime.active_cursor()==nullptr?0U:s.runtime.active_cursor()==s.target?1U:2U;
        const bool state=s.actor.money()==money && s.target->success==success && std::vector<std::uint8_t>(s.target->packet.counters.begin(),s.target->packet.counters.end())==unhex(final_counters);
        if(!state || s.events!=unhex(events) || s.inventory_state()!=unhex(final_inventory) || s.sequence(s.runtime.active())!=active || s.sequence(s.runtime.finished())!=finished || s.sequence(s.quest_pool.available())!=free || actual_cursor!=cursor) {
            std::cerr<<kind<<" Actor chain "<<scenario<<" state "<<state<<" events "<<(s.events==unhex(events))<<" inventory "<<(s.inventory_state()==unhex(final_inventory))<<" cursor "<<actual_cursor<<'/'<<cursor<<'\n';throw std::runtime_error("complete original Actor chain differs");
        }
        s.verify();releases+=static_cast<unsigned>(s.released.size());++chains;
    }}
    // Ownership-invalid callback is rejected after its DB effect; never send a
    // stale client notice or release a replacement allocation.
    {Services s;std::vector<std::uint8_t> bytes(24*5,0);bytes[0]=1;bytes[1]=1;bytes[2]=1;bytes[3]=2;s.load(bytes,1);s.break_identity=true;bool rejected=false;try {(void)s.inventory.find_item(1,1,true);}catch(const std::logic_error&){rejected=true;}check(rejected && s.events.front()=='B' && s.inventory_cache.inventory().size()==3,"stale Actor borrow was not rejected at DB boundary");}
    check(flows==512 && chains==384 && releases>680,"missing Actor fixture coverage");std::cout<<flows<<" original consumption flows and "<<chains<<" complete Actor/runtime/reward/request/progress/timer chains; "<<releases<<" direct pool releases; replicas, mirrors, callbacks and cursors matched\n";
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}}
