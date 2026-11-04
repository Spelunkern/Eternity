#include "eternity/game/quest_requests_completion.hpp"
#include "eternity/game/quest_runtime.hpp"
#include "eternity/game/quest_rewards.hpp"
#include "eternity/game/quest_replica.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace eternity::game;
namespace {
void check(bool ok,const char* s) {if(!ok) throw std::runtime_error(s);}
std::vector<std::uint8_t> unhex(const std::string& s) {std::vector<std::uint8_t> p;if(s=="-") return p;for(std::size_t i=0;i<s.size();i+=2) p.push_back(static_cast<std::uint8_t>(std::stoul(s.substr(i,2),nullptr,16)));return p;}
void append(std::vector<std::uint8_t>& p,std::uint32_t n) {for(unsigned i=0;i<4;++i) p.push_back(static_cast<std::uint8_t>(n>>(8*i)));}
struct Pool final:QuestRuntimePoolServices {std::unique_ptr<RestoredQuest> construct() override {return std::make_unique<RestoredQuest>();}};
struct Services final:QuestRuntimeWorld,QuestRewardWorld {
    const QuestDefinitions& definitions;Pool allocation;QuestRuntimePool pool{allocation};QuestRuntime runtime{7,pool};
    QuestReplicaPool cached_pool;QuestReplica replica{7,cached_pool};RestoredQuest* target=nullptr;RestoredQuest* other=nullptr;
    std::uint32_t now=0,money=17;unsigned mode=0;bool query=false;std::vector<std::uint8_t> events;
    explicit Services(const QuestDefinitions& d):definitions(d) {}
    void event(char c,std::initializer_list<std::uint32_t> fields={}) {events.push_back(static_cast<std::uint8_t>(c));for(auto f:fields) append(events,f);}
    void packet(char c,const eternity::net::Packet& p) {event(c,{static_cast<std::uint32_t>(p.size())});events.insert(events.end(),p.begin(),p.end());}
    std::uint32_t tick_ms() {event('T');if(mode==2) {target->definition=definitions.find(2);target->start_tick=0;}return now;}
    bool find_required_items(RestoredQuest& q,bool consume) {check(&q==target && consume,"invalid selection inventory binding");event('Q',{consume?1U:0U});if(mode==5) {q.definition=definitions.find(2);q.packet.counters={7,8,9};}return query;}
    void send_database(const eternity::net::Packet& p) override {
        packet('B',p);
        if(p[0]==2 && p[1]==10) check(replica.apply(p)==InventoryReplicaResult::applied,"completion DB transition rejected");
        if(mode==1) {check(runtime.find_active(2)==other,"reentrant lookup lost other quest");event('J',{2});}
    }
    void add_experience(std::uint32_t source,std::int32_t amount,bool bypass) override {event('X',{source,static_cast<std::uint32_t>(amount),bypass?1U:0U});}
    void fill_game_log_main(std::span<std::uint8_t> p) override {event('M');std::fill(p.begin()+2,p.end(),0);if(mode==4) {target->packet.id=9;target->definition=definitions.find(2);}}
    std::optional<QuestRewardItemReceipt> receive_item(std::uint8_t type,std::uint8_t id,std::uint8_t count) override {event('I',{type,id,count});return {};}
    void send_self(const eternity::net::Packet& p) override {packet('C',p);if(mode==3) {target->definition=definitions.find(2);target->success=2;}}
    void remove_quest(RestoredQuest& q,bool success) override {runtime.remove(q,success,*this);}
    void send_game_log(const eternity::net::Packet& p) override {packet('G',p);}
    std::string sequence(std::span<RestoredQuest* const> values) const {std::string s;for(auto* q:values) {if(!s.empty()) s+=',';s+=q==target?'1':'2';}return s.empty()?"-":s;}
};
}
int main(int argc,char** argv) {try {
    check(argc==2,"fixture required");std::ifstream input(argv[1]);check(input.good(),"cannot read fixture");unsigned rows=0;
    for(std::string line;std::getline(input,line);) {
        if(line.empty() || line[0]=='#') continue;
        std::istringstream r(line);char kind;unsigned scenario,now,mode,query,context,success,selection,lookup,start,pending,money,id,binding,final_start,final_success,cursor;std::string block,counters,events,final_counters,active,finished,free;
        r>>kind>>scenario>>now>>mode>>query>>context>>success>>selection>>lookup>>block>>start>>counters>>pending>>events>>money>>id>>binding>>final_start>>final_counters>>final_success>>active>>finished>>free>>cursor;check(!r.fail(),"malformed completion fixture");
        const auto definitions=QuestDefinitions::decode(unhex(block));Services world(definitions);world.now=now;world.mode=mode;world.query=query!=0;
        RestoredQuest seed;seed.definition=definitions.find(1);seed.packet.id=1;seed.start_tick=start;seed.success=static_cast<std::uint8_t>(pending);
        const auto c=unhex(counters);std::copy(c.begin(),c.end(),seed.packet.counters.begin());world.target=world.runtime.load_active(seed);check(world.target!=nullptr,"cannot allocate target quest");
        seed.packet.id=2;world.other=world.runtime.load_active(seed);check(world.other!=nullptr,"cannot allocate other quest");
        check(world.replica.load({1,0,{}}) && world.replica.load({2,0,{}}),"cannot load completion replica");
        auto award=[&](RestoredQuest& q,std::uint8_t v,std::uint32_t ctx) {award_quest_reward(q,v,ctx,7,world.money,world);};
        QuestCompletionOwners owners(world.runtime,world,world,award);
        if(kind=='E') quest_requests_complete(*world.target,context,success!=0,owners);
        else {check(kind=='S',"unknown completion entry");quest_requests_complete_selection(static_cast<std::uint16_t>(lookup),static_cast<std::uint8_t>(selection),owners);}
        auto* q=world.target;const auto actual_cursor=world.runtime.active_cursor()==nullptr?0U:world.runtime.active_cursor()==world.target?1U:2U;
        const bool state=world.money==money && q->packet.id==id && q->definition==definitions.find(static_cast<std::uint16_t>(binding+1)) && q->start_tick==final_start && std::vector<std::uint8_t>(q->packet.counters.begin(),q->packet.counters.end())==unhex(final_counters) && q->success==final_success;
        if(!state || world.events!=unhex(events) || world.sequence(world.runtime.active())!=active || world.sequence(world.runtime.finished())!=finished || world.sequence(world.pool.available())!=free || actual_cursor!=cursor) {
            std::cerr<<kind<<" completion "<<scenario<<" state "<<state<<" events "<<(world.events==unhex(events))<<" cursor "<<actual_cursor<<'/'<<cursor<<'\n';
            if(world.events!=unhex(events)) {auto expected=unhex(events);std::size_t i=0;while(i<world.events.size() && i<expected.size() && world.events[i]==expected[i]) ++i;std::cerr<<"first trace difference "<<i<<" sizes "<<world.events.size()<<'/'<<expected.size()<<'\n';}
            throw std::runtime_error("original completion/owner composition mismatch");
        }
        ++rows;
    }
    check(rows==1536,"missing original completion cases");std::cout<<rows<<" complete original QuestEnd/Select paths matched actual canonical QuestRuntime/QuestReward owners, packets, callbacks and cursor\n";
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}}
