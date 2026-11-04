#include "quest_start_items_actor_fixture.hpp"
#include "eternity/game/vertical_quests.hpp"
#include <iostream>
using namespace quest_actor_test;
namespace {
struct PoolFactory final:QuestRuntimePoolServices {std::unique_ptr<RestoredQuest> construct() override {return std::make_unique<RestoredQuest>();}};
struct NpcServices final:ZoneNpcServices {
    bool fail_broadcast=false;
    Npc* allocate() override {throw std::logic_error("unexpected allocation");}
    std::uint32_t next_id() override {throw std::logic_error("unexpected id");}
    std::uint32_t tick_ms() const noexcept override {return 100;}
    void invalid_guard_position(const MapNpc&) override {throw std::logic_error("unexpected guard");}
    void send_cell_view(CellCoordinates,const net::Packet&) override {if(fail_broadcast) throw std::runtime_error("NPC broadcast failed");}
    std::unique_ptr<NpcBroadcast> allocate_broadcast(const net::Packet&,std::optional<std::uint8_t>) override {throw std::logic_error("unexpected broadcast");}
};
struct Services final:VerticalQuestPorts,QuestRewardWorld,QuestRuntimeWorld {
    Actor& actor;QuestRuntime& runtime;Scene& scene;QuestReplicaPool replica_pool;QuestReplica replica;
    std::vector<unsigned> db;std::vector<unsigned> order;std::function<void()> on_add;std::array<std::uint8_t,144> start_log{};unsigned locks=0;std::int32_t experience=0;
    Services(Actor& a,QuestRuntime& r,Scene& s):actor(a),runtime(r),scene(s),replica(a.account_id(),replica_pool) {}
    void send_database(Actor& a,const net::Packet& p) override {check(&a==&actor,"wrong actor");send_database(p);}
    void send_database(const net::Packet& p) override {auto op=word(p,0,2);db.push_back(op);order.push_back(op);check(scene.packets.size()==(op==0xa01?0U:op==0xa05?1U:3U),"database/client ordering differs");check(replica.apply(p)==InventoryReplicaResult::applied,"quest replica rejected packet");if(op==0xa01 && on_add) on_add();}
    void send_client(Actor& a,const net::Packet& p) override {check(&a==&actor,"wrong client actor");order.push_back(word(p,0,2));a.send_self(p);}
    std::uint32_t local_make_time(Actor&) override {return 1;}
    std::uint32_t tick_ms(Actor&) override {return 100;}
    std::uint32_t quest_tick_ms() override {return 100;}
    void lock_quests(Actor&) override {++locks;}
    void unlock_quests(Actor&) noexcept override {--locks;}
    void fill_game_log_main(Actor&,std::span<std::uint8_t> p) override {std::fill(p.begin(),p.end(),0);}
    void send_game_log(Actor&,const net::Packet& p) override {order.push_back(word(p,0,2));}
    QuestRewardWorld& reward_world(Actor& a) override {check(&a==&actor,"wrong reward actor");return *this;}
    QuestStartLogContext start_log_context(Actor&) override {return {};}
    void send_start_log(Actor&,const std::array<std::uint8_t,144>& p) override {start_log=p;order.push_back(0x501);}
    void add_experience(std::uint32_t,std::int32_t n,bool) override {experience+=n;order.push_back(0xffff);}
    void fill_game_log_main(std::span<std::uint8_t> p) override {fill_game_log_main(actor,p);}
    std::optional<QuestRewardItemReceipt> receive_item(std::uint8_t t,std::uint8_t,std::uint8_t) override {check(t==0,"unexpected reward item");return {};}
    void send_self(const net::Packet& p) override {order.push_back(word(p,0,2));actor.send_self(p);}
    void remove_quest(RestoredQuest& q,bool success) override {runtime.remove(q,success,*this);}
    void send_game_log(const net::Packet& p) override {send_game_log(actor,p);}
};
// Encode a single record in CQuestData::Load order, with one NPC-gated mob quest.
net::Packet quest_data() {
    std::array<std::uint8_t,464> q{};q[0]=1;q[0x104]=100;q[0x106]=2;q[0x10a]=1;
    q[0x138]=1;q[0x139]=2;q[0x13a]=9;q[0x147]=2;q[0x148]=2;q[0x14a]=9;
    q[0x156]=3;q[0x15a]=1;q[0x174]=10;
    net::Packet p;append(p,1);auto copy=[&](std::size_t at,std::size_t n){p.insert(p.end(),q.begin()+static_cast<std::ptrdiff_t>(at),q.begin()+static_cast<std::ptrdiff_t>(at+n));};
    copy(0,2);append(p,0);append(p,0);copy(0x102,2);copy(0x104,2);copy(0x106,10);copy(0x110,4);copy(0x114,3);copy(0x118,2);copy(0x11a,7);copy(0x124,20);copy(0x138,17);copy(0x14a,12);copy(0x156,2);copy(0x15a,1);copy(0x158,2);copy(0x15b,3);
    for(std::size_t at=0x160;at<0x1c0;at+=32) {copy(at,5);copy(at+8,9);copy(at+20,12);}for(int i=0;i<7;++i) append(p,0);return p;
}
}
int main() {try {
    auto defs=std::make_shared<QuestDefinitions>(QuestDefinitions::decode(quest_data()));auto items=catalog(5,{1});auto item_pool=std::make_shared<GroundItemPool>();Clock clock;auto ids=std::make_shared<ItemIds>(3,clock);Scene scene;EnchantValues enchant;MobAttackTemplates mobs;
    auto actor=std::make_shared<Actor>(9001,EffectStatus{},PlayerResources{},std::vector<PersistedEffect>{},scene,ActorEquipmentRules{*items,enchant,mobs});
    actor->account_id()=77;actor->status().level=20;actor->appearance().identity.job=0;
    PoolFactory factory;auto pool=std::make_shared<QuestRuntimePool>(factory);auto runtime=std::make_shared<QuestRuntime>(77,*pool);Services services(*actor,*runtime,scene);
    NpcServices npc_services;NpcDefinition npc_definition,replacement_definition;NpcPool npc_pool;ZoneNpcs zone(MapGeometry(81),npc_services);
    npc_definition.record[0]=2;npc_definition.record[20]='A';replacement_definition.record[20]='B';
    auto* npc=npc_pool.allocate();npc->id=99;npc->type=2;npc->type_id=9;npc->definition=&npc_definition;check(zone.enter(*npc),"NPC entry failed");
    auto guard=std::make_shared<VerticalQuestNpcGuard>(zone,npc_pool);Npc* replacement=nullptr;
    std::uint8_t last=0;std::uint32_t deadline=0;
    VerticalQuests quests({items,item_pool,ids,defs,pool,actor,runtime},{last,deadline},zone,guard,services);
    services.on_add=[&] {
        check(guard->retire(*npc),"nested NPC retirement failed");
        replacement=npc_pool.allocate();check(replacement!=npc,"leased NPC was recycled during callback");
        replacement->id=99;replacement->type=2;replacement->type_id=9;replacement->definition=&replacement_definition;
        check(zone.enter(*replacement),"replacement NPC entry failed");
        check(!quests.start(1,99),"nested duplicate start accepted");
    };
    check(!quests.start(1,98),"missing NPC accepted");check(quests.start(1,99),"quest not accepted");check(runtime->active().size()==1 && runtime->mob_kill_quests()==1,"accept did not use actual runtime");
    check(services.start_log[112]=='A',"start log used replacement NPC");
    auto* recycled=npc_pool.allocate();check(recycled==npc,"retired NPC not released after lease");npc_pool.release(*recycled);
    quests.end(1,99);check(runtime->active().size()==1,"unfinished quest completed");
    MobQuestLinks links({{}, {}, {}, {1}});quests.kill_mob(3,links);check(runtime->active().front()->packet.counters[0]==1,"actual quest counter not updated");check(runtime->active().size()==1,"NPC-gated quest completed early");
    quests.end(1,98);check(runtime->active().size()==1,"wrong NPC completed quest");quests.end(1,99);
    check(runtime->active().empty() && runtime->finished().size()==1 && runtime->mob_kill_quests()==0,"completion did not remove actual quest");check(services.experience==20,"experience callback mismatch");check(services.replica.active().empty(),"DB replica retains completed quest");
    check(scene.packets.size()==3 && word(scene.packets[0],0,2)==0x902 && word(scene.packets[1],0,2)==0x905 && word(scene.packets[2],0,2)==0x903,"client packet order differs");
    check(services.db==std::vector<unsigned>({0xa01,0xa05,0xa02}),"database ordering differs");check(services.locks==0,"quest lock leaked");
    check(guard->retire(*replacement),"NPC retirement failed");
    auto* failed=npc_pool.allocate();failed->id=101;check(zone.enter(*failed),"failure NPC entry failed");
    auto lease=guard->acquire(101);npc_services.fail_broadcast=true;bool threw=false;
    try {guard->retire(*failed);} catch(const std::runtime_error&) {threw=true;}
    npc_services.fail_broadcast=false;check(threw,"retirement failure was hidden");lease.reset();
    auto* other=npc_pool.allocate();check(other!=failed,"failed retirement recycled NPC");npc_pool.release(*other);
    // ZoneNpcs::leave unlinks before broadcasting and did not clear the cell on failure.
    failed->cell.reset();npc_pool.release(*failed);
    std::cout<<"Actual Actor quest acceptance, mob progress and NPC completion passed\n";
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}}
