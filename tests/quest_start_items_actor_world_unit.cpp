#include "quest_start_items_actor_fixture.hpp"
#include "eternity/game/quest_start_items_actor_world.hpp"
#include <fstream>
#include <iostream>
using namespace quest_actor_test;
namespace {
struct LiveClock final:ItemIdClock {
    std::function<std::uint32_t()> read_tick;
    std::uint32_t ticks_ms() override {check(bool(read_tick),"Shared UID clock was not bound");return read_tick();}
    std::uint32_t packed_local_calendar() override {return quest_start_log_time(2026,11,3,15,30,40);}
};
struct User {std::uint8_t last=7;std::uint32_t deadline=0;Actor actor;
    User(Scene& scene,const ItemDefinitions& items,const EnchantValues& enchant,const MobAttackTemplates& mobs):actor(9001,{}, {},{},scene,{items,enchant,mobs}) {actor.status().level=20;actor.status().skills.max_bags=1;actor.appearance().identity.country=0;actor.appearance().identity.grow=0;actor.appearance().identity.job=1;}
};
struct Factory final:QuestRuntimePoolServices {bool enabled=true;std::unique_ptr<RestoredQuest> construct() override {return enabled?std::make_unique<RestoredQuest>():nullptr;}};
struct Root final:QuestStartItemsActorWorldPorts {
    User& user;std::shared_ptr<const QuestDefinitions> definitions;std::shared_ptr<QuestRuntime> runtime;Ports delivery;
    unsigned profile=0,clocks=0,inventory=1;std::uint32_t now=0,npc_type=2;std::string npc_name="NPC";net::Packet events;std::function<void()> release_user;
    Root(User& u,std::shared_ptr<const QuestDefinitions> d,std::shared_ptr<QuestRuntime> r,std::uint32_t billing):user(u),definitions(std::move(d)),runtime(std::move(r)),delivery(u.actor.status().skills.max_bags,billing) {}
    void event(char c,std::initializer_list<std::uint32_t> v={}) {events.push_back(static_cast<std::uint8_t>(c));for(auto n:v) append(events,n);}
    std::optional<QuestStartNpc> find_start_npc(Actor& actor,std::uint32_t target) override {
        event('N',{target});check(&actor==&user.actor,"Wrong actual user lease");
        if(profile==0) actor.status().level=0;
        else if(profile==1) {RestoredQuest q;q.packet.id=9;q.definition=definitions->find(2);q.success=1;check(runtime->load_finished(q)!=nullptr,"Prerequisite callback allocation failed");}
        else if(profile==4) {RestoredQuest q;q.packet.id=7;q.definition=definitions->find(1);check(runtime->load_active(q)!=nullptr,"Duplicate callback allocation failed");}
        else if(profile==6 && release_user) release_user();
        if(profile==5) return {};return QuestStartNpc{99,2,9};
    }
    bool require_start_item(Actor&,std::uint8_t type,std::uint8_t id,bool consume) override {event('I',{type,id,consume?1u:0u});if(profile==8) return false;if(consume) --inventory;return true;}
    std::uint32_t local_make_time(Actor& actor) override {event('M');return delivery.local_make_time(actor);}
    std::uint32_t tick_ms(Actor&) override {const auto tick=now++;event('T',{tick});if(clocks++==0) {if(profile==2) user.deadline=tick;else if(profile==3) user.last=8;}return tick;}
    void send_database(Actor& actor,const net::Packet& p) override {
        event('B');if(word(p,0,2)==0xa01) {append(events,runtime->active().size());append(events,static_cast<std::uint32_t>(runtime->mob_kill_quests()));append(events,static_cast<std::uint32_t>(runtime->player_kill_quests()));}
        events.insert(events.end(),p.begin(),p.end());delivery.send_database(actor,p);
    }
    void send_client(Actor& actor,const net::Packet& p) override {event('C');events.insert(events.end(),p.begin(),p.end());delivery.send_client(actor,p);if(profile==7) {actor.status().level=1;npc_type=9;npc_name="LATE";}}
    QuestStartLogContext start_log_context(Actor& actor) override {event('P');QuestStartLogContext c;c.user_id=actor.account_id();c.character_id=actor.id();c.level=actor.status().level;c.system_time=0x12345678;return c;}
    QuestStartLogNpc start_log_npc(Actor&,const QuestStartNpc&) override {return {npc_type,npc_name};}
    void send_start_log(Actor&,const std::array<std::uint8_t,144>& p) override {event('L');events.insert(events.end(),p.begin(),p.end());}
    net::Packet snapshot() {
        net::Packet p;append(p,runtime->active().size());append(p,static_cast<std::uint32_t>(runtime->mob_kill_quests()));append(p,static_cast<std::uint32_t>(runtime->player_kill_quests()));append(p,user.actor.status().level,2);append(p,user.last,1);append(p,user.deadline);append(p,inventory,1);append(p,runtime->finished().size());
        const auto* q=runtime->active().empty()?nullptr:runtime->active().front();append(p,q?q->packet.id:0,2);append(p,q?q->start_tick:0);append(p,q?q->packet.end_tick:0);append(p,q?q->success_tick:0);append(p,user.actor.inventory().size());return p;
    }
};
}
int main(int argc,char** argv) {try {
    check(argc==2,"Expected original world callback fixture");std::ifstream input(argv[1]);check(input.good(),"Missing fixture");unsigned cases=0,starts=0,grants=0;
    for(std::string line;std::getline(input,line);) {
        if(line.empty() || line[0]=='#') continue;unsigned id,profile,country,family,grow,job,row,started;std::uint32_t tick,deadline,target;std::string block,state,events;std::istringstream text(line);text>>id>>profile>>country>>family>>grow>>job>>block>>tick>>deadline>>row>>target>>started>>state>>events;check(!text.fail(),"Malformed callback fixture");
        auto definitions=std::make_shared<QuestDefinitions>(QuestDefinitions::decode(decode(block)));auto items=catalog(5,{1,2,3});auto pool=std::make_shared<GroundItemPool>();LiveClock clock;auto ids=std::make_shared<ItemIds>(3,clock);Scene scene;EnchantValues enchant;MobAttackTemplates mobs;
        auto user=std::make_shared<User>(scene,*items,enchant,mobs);user->actor.account_id()=77+id;user->actor.appearance().identity.country=static_cast<std::uint8_t>(country);user->actor.appearance().identity.family=static_cast<std::uint8_t>(family);user->actor.appearance().identity.grow=static_cast<std::uint8_t>(grow);user->actor.appearance().identity.job=static_cast<std::uint8_t>(job);user->deadline=deadline;auto actor=std::shared_ptr<Actor>(user,&user->actor);std::weak_ptr<User> weak=user;
        Factory factory;factory.enabled=profile!=9 && profile!=10;auto quest_pool=std::make_shared<QuestRuntimePool>(factory);auto runtime=std::make_shared<QuestRuntime>(77+id,*quest_pool);Root root(*user,definitions,runtime,77+id);root.profile=profile;root.now=tick;clock.read_tick=[&] {return root.tick_ms(root.user.actor);};
        root.release_user=[&] {actor.reset();user.reset();check(!weak.expired(),"Concrete bridge failed to retain aliasing user lease");};
        {
            QuestStartItemsActorWorld world({items,pool,ids,definitions,quest_pool,actor,runtime},{user->last,user->deadline},root);
            const auto result=world.request(static_cast<std::uint16_t>(row),target);check(result==(started!=0),"Original callback admission mismatch "+std::to_string(id));check(root.events==decode(events),"Complete original callback/world transport mismatch "+std::to_string(id));check(root.snapshot()==decode(state),"Actual runtime/user callback state mismatch "+std::to_string(id));
            check(root.delivery.quest_cache.active().size()==started,"Actual quest cache delivery mismatch");check(root.delivery.cache.inventory().size()==root.user.actor.inventory().size(),"Actual Actor/inventory replica grant mismatch");grants+=static_cast<unsigned>(root.user.actor.inventory().size());starts+=started;++cases;
        }
        if(profile==6) check(weak.expired(),"Aliasing user lease retained beyond bridge destruction");
    }
    check(cases==224 && starts>70 && grants>100,"Insufficient callback bridge coverage");std::cout<<cases<<" complete original request/world callback traces / "<<starts<<" starts / "<<grants<<" actual Actor grants matched through actual owner bridge; live prerequisite/clock/log snapshots and aliasing user lease passed\n";
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}}
