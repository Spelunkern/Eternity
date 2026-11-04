#include "quest_start_items_actor_fixture.hpp"
#include "eternity/game/quest_start.hpp"
#include <bit>
#include <fstream>
#include <iostream>
using namespace quest_actor_test;
namespace {
struct Factory final:QuestRuntimePoolServices {
    bool enabled=true;RestoredQuest* last=nullptr;std::function<void()> on_construct;
    std::unique_ptr<RestoredQuest> construct() override {if(on_construct) on_construct();if(!enabled) {last=nullptr;return {};}auto q=std::make_unique<RestoredQuest>();last=q.get();return q;}
};
struct FlowPorts final:Ports {
    net::Packet& trace;
    FlowPorts(std::int32_t& bags,std::uint32_t account,net::Packet& t):Ports(bags,account),trace(t) {}
    std::uint32_t tick_ms(Actor& actor) override {const auto v=Ports::tick_ms(actor);trace.push_back('T');append(trace,v);return v;}
};
struct World final:QuestStartWorld {
    const QuestDefinition& definition;QuestRuntime& runtime;Actor& actor;QuestStartItemsActor& grants;QuestStartItemsActorAdd& admission;FlowPorts& ports;net::Packet& events;
    unsigned npc=0,inventory=0,notifications=0;std::array<std::uint8_t,144> log{};
    World(const QuestDefinition& d,QuestRuntime& r,Actor& a,QuestStartItemsActor& g,QuestStartItemsActorAdd& q,FlowPorts& p,net::Packet& e):definition(d),runtime(r),actor(a),grants(g),admission(q),ports(p),events(e) {}
    void event(char c,std::initializer_list<std::uint32_t> v={}) {events.push_back(static_cast<std::uint8_t>(c));for(auto n:v) append(events,n);}
    std::optional<QuestStartNpc> find_npc(std::uint32_t target) override {event('N',{target});if(!npc) return {};return QuestStartNpc{99,2,9};}
    bool active(std::uint16_t id) override {event('A',{id});return runtime.find_active(id)!=nullptr;}
    bool finished(std::uint16_t id) override {event('F',{id});return std::any_of(runtime.finished().begin(),runtime.finished().end(),[&](auto* q){return q->packet.id==id;});}
    // The separate request/inventory requirement owner remains an explicit port.
    // This fixture retains its original input/consumption witness; it is not a
    // replacement implementation of that owner's QuestFindItem body.
    bool find_item(std::uint8_t type,std::uint8_t id,bool consume) override {event('I',{type,id,consume?1u:0u});if(!inventory) return false;if(consume) --inventory;return true;}
    std::uint32_t tick_ms() override {return ports.tick_ms(actor);}
    bool add(const QuestStartRecord& q) override {check(&q==&definition.record(),"Lost positional canonical definition");return admission.add(1);}
    void prepare_start_log(const QuestStartRecord& q,const std::optional<QuestStartNpc>& npc) override {event('P');std::optional<QuestStartLogNpc> context;if(npc) context=QuestStartLogNpc{npc->type,"NPC"};log=quest_start_log_packet({},q,context);}
    void send_self(const std::array<std::uint8_t,8>& p) override {++notifications;event('C');events.insert(events.end(),p.begin(),p.end());ports.send_client(actor,net::Packet(p.begin(),p.end()));}
    void log_start(const QuestStartRecord&,const std::optional<QuestStartNpc>&) override {event('L');events.insert(events.end(),log.begin(),log.end());}
    bool give_item(std::uint8_t type,std::uint8_t id,std::uint8_t count) override {event('G',{type,id,count});return grants.grant(type,id,count);}
};
struct FailurePorts final:Ports {
    bool fail_tick=false;
    using Ports::Ports;
    std::uint32_t tick_ms(Actor& actor) override {if(fail_tick) throw std::runtime_error("Clock unavailable");return Ports::tick_ms(actor);}
};
void quest_failures(std::shared_ptr<const QuestDefinitions> definitions) {
    Scene scene;EnchantValues enchant;MobAttackTemplates mobs;auto items=catalog(5,{1});
    auto actor=std::make_shared<Actor>(9001,EffectStatus{},PlayerResources{},std::vector<PersistedEffect>{},scene,ActorEquipmentRules{*items,enchant,mobs});actor->account_id()=77;
    Factory factory;factory.enabled=false;auto pool=std::make_shared<QuestRuntimePool>(factory);auto runtime=std::make_shared<QuestRuntime>(77,*pool);std::int32_t bags=1;FailurePorts ports(bags,77);
    QuestStartItemsActorAdd adapter({definitions,pool,actor,runtime},ports);
    check(!adapter.add(0) && !adapter.add(2) && pool->construction_attempts()==0,"Invalid positional row allocated quest");
    check(!adapter.add(1) && runtime->active().empty() && ports.packets.empty(),"Ordinary quest allocation failure emitted work");
    factory.enabled=true;ports.fail_tick=true;bool failed=false;try {(void)adapter.add(1);}catch(const std::runtime_error&) {failed=true;}
    check(failed && pool->available().size()==1 && runtime->active().empty(),"Pre-insertion clock failure stranded quest");
    const auto attempts=pool->construction_attempts();ports.fail_tick=false;ports.fail_database=true;failed=false;try {(void)adapter.add(1);}catch(const std::runtime_error&) {failed=true;}
    check(failed && runtime->active().size()==1 && pool->available().empty() && pool->construction_attempts()==attempts,"Post-insertion transport failure lost quest ownership or reallocated");
    ports.fail_database=false;check(adapter.add(1) && runtime->active().size()==2,"Quest add frame not restored after failure");
    const auto type=definitions->find(1)->end_type();check(runtime->mob_kill_quests()==(type==2?2:0) && runtime->player_kill_quests()==(type==3?2:0),"Canonical quest accounting incremented twice");
}

}
int main(int argc,char** argv) {try {
    check(argc==2,"Expected request/add original fixture");std::ifstream input(argv[1]);check(input.good(),"Missing flow fixture");unsigned cases=0,starts=0,total_grants=0;std::shared_ptr<const QuestDefinitions> first_definitions;
    for(std::string line;std::getline(input,line);) {
        if(line.empty() || line[0]=='#') continue;
        unsigned id,last,npc,has_active,has_finished,inventory,allocation,started,remaining;std::uint32_t now,billing,mob,player,deadline,target;std::string block,expected_state,expected_events;
        std::istringstream row(line);row>>id>>block>>now>>billing>>mob>>player>>deadline>>last>>target>>npc>>has_active>>has_finished>>inventory>>allocation>>started>>remaining>>expected_state>>expected_events;check(!row.fail(),"Malformed original flow fixture");
        auto definitions=std::make_shared<QuestDefinitions>(QuestDefinitions::decode(decode(block)));if(!first_definitions) first_definitions=definitions;const auto* definition=definitions->find(1);check(definition!=nullptr,"Missing quest");
        std::vector<std::uint64_t> item_ids;for(std::size_t p=0x13e;p<=0x144;p+=3) if(definition->record()[p] && definition->record()[p+1]) item_ids.push_back(definition->record()[p+1]);std::sort(item_ids.begin(),item_ids.end());item_ids.erase(std::unique(item_ids.begin(),item_ids.end()),item_ids.end());
        auto items=catalog(4,item_ids);Scene scene;EnchantValues enchant;MobAttackTemplates mobs;Clock clock;
        auto pool=std::make_shared<GroundItemPool>(1,[&]()->std::unique_ptr<GroundItem> {if(id%7==0) return {};return std::make_unique<GroundItem>();});auto uid=std::make_shared<ItemIds>(3,clock);const auto external_before=uid->next();
        auto actor=std::make_shared<Actor>(9001,EffectStatus{},PlayerResources{},std::vector<PersistedEffect>{},scene,ActorEquipmentRules{*items,enchant,mobs});actor->account_id()=billing;
        Factory factory;auto quest_pool=std::make_shared<QuestRuntimePool>(factory);auto runtime=std::make_shared<QuestRuntime>(billing,*quest_pool);RestoredQuest initial;initial.definition=definition;initial.packet.id=definition->id();
        if(has_active) check(runtime->load_active(initial)!=nullptr,"Existing quest load failed");if(has_finished) check(runtime->load_finished(initial)!=nullptr,"Finished quest load failed");
        const auto initial_active=runtime->active().size();runtime->mob_kill_quests()=std::bit_cast<std::int32_t>(mob);runtime->player_kill_quests()=std::bit_cast<std::int32_t>(player);factory.last=nullptr;factory.enabled=allocation!=0;
        actor->status().skills.max_bags=1;net::Packet events;factory.on_construct=[&] {events.push_back('O');};FlowPorts ports(actor->status().skills.max_bags,billing,events);ports.now=now;
        const auto counts=[&] {net::Packet p;append(p,runtime->active().size()-initial_active);append(p,static_cast<std::uint32_t>(runtime->mob_kill_quests()));append(p,static_cast<std::uint32_t>(runtime->player_kill_quests()));return p;};
        ports.database_callback=[&](Actor& actual,const net::Packet& p) {
            if(word(p,0,2)==0xa01) {events.push_back('B');const auto c=counts();events.insert(events.end(),c.begin(),c.end());events.insert(events.end(),p.begin(),p.end());check(runtime->active().size()==initial_active+1,"Quest DB before canonical insertion");}
            else {auto held=actual.retained_inventory_item(p[18],p[19]);check(held && held->pool==pool.get(),"Grant used another pool");}
        };
        {
            QuestStartItemsActor grant({items,pool,uid,actor},ports);QuestStartItemsActorAdd add({definitions,quest_pool,actor,runtime},ports);World world(*definition,*runtime,*actor,grant,add,ports,events);world.npc=npc;world.inventory=inventory;
            QuestStartUser user{20,0,0,1,static_cast<std::uint8_t>(last),deadline,{}};const bool actual=quest_start_request(user,std::span(&definition->record(),1),1,target,world);
            check(actual==(started!=0) && world.notifications==started,"Actual start return mismatch "+std::to_string(id));check(events==decode(expected_events),"Original start/add/log trace mismatch "+std::to_string(id));check(world.inventory==remaining,"Requirement consumption boundary mismatch");
            auto state=counts();RestoredQuest empty;const auto& q=factory.last?*factory.last:empty;append(state,q.packet.id);append(state,q.start_tick);append(state,q.packet.end_tick);append(state,q.success_tick);append(state,q.success,1);for(auto c:q.packet.counters) append(state,c,1);append(state,q.definition==definition?1:0);check(state==decode(expected_state),"Canonical QuestRuntime state/accounting mismatch "+std::to_string(id));
            check(ports.quest_cache.active().size()==started,"Actual QuestReplica add missing");check(actor->inventory().size()==ports.cache.inventory().size(),"Actor/InventoryReplica new grants diverged");
            check(uid->next()==external_before+(actor->inventory().size()+1)*65536ULL,"Quest grants bypassed the same live shared UID");
            if(started) {check(!ports.packets.empty() && ports.packets[0]=='B' && word(ports.packets,1,2)==0xa01,"Actual item delivery preceded quest DB add");++starts;}
            total_grants+=static_cast<unsigned>(actor->inventory().size());
        }
        actor.reset();check(pool->statistics().available==pool->statistics().owned,"Actor-owned items stranded at shutdown");++cases;
    }
    quest_failures(first_definitions);
    check(cases==640 && starts>100 && total_grants>100,"Insufficient concrete start/add/grant coverage");std::cout<<cases<<" original request/add/start traces / "<<starts<<" starts / "<<total_grants<<" real retained Actor grants; canonical QuestRuntime accounting and live shared UID passed\n";
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}}
