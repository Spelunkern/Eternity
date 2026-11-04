#include "eternity/game/quest_start.hpp"
#include "eternity/game/quest_start_add.hpp"
#include "eternity/game/quest_start_log.hpp"
#include "eternity/game/quest_replica.hpp"
#include <bit>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace eternity;using namespace eternity::game;
namespace {
void check(bool b,const std::string& s) {if(!b) throw std::runtime_error(s);}
net::Packet decode(const std::string& s) {net::Packet r;if(s=="-") return r;for(std::size_t i=0;i<s.size();i+=2) r.push_back(static_cast<std::uint8_t>(std::stoul(s.substr(i,2),nullptr,16)));return r;}
void append(net::Packet& p,std::uint32_t v,unsigned n=4) {for(unsigned i=0;i<n;++i) p.push_back(static_cast<std::uint8_t>(v>>(8*i)));}
struct World final:QuestStartWorld,QuestStartAddWorld {
    const QuestDefinition* definition=nullptr;RestoredQuest quest;std::uint32_t now=0,billing=0,active_count=0;std::int32_t mob=0,player=0;
    unsigned npc=0,has_active=0,has_finished=0,inventory=0,allocation=0,notifications=0;net::Packet events;std::array<std::uint8_t,144> log{};
    QuestStartAddUser user{billing,mob,player};QuestReplicaPool pool;
    void event(char c,std::initializer_list<std::uint32_t> values={}) {events.push_back(static_cast<std::uint8_t>(c));for(auto v:values) append(events,v);}
    net::Packet counts() const {net::Packet p;append(p,active_count);append(p,static_cast<std::uint32_t>(mob));append(p,static_cast<std::uint32_t>(player));return p;}
    net::Packet snapshot() const {auto p=counts();append(p,quest.packet.id);append(p,quest.start_tick);append(p,quest.packet.end_tick);append(p,quest.success_tick);append(p,quest.success,1);for(auto c:quest.packet.counters) append(p,c,1);append(p,quest.definition==definition?1:0);return p;}
    std::optional<QuestStartNpc> find_npc(std::uint32_t handle) override {event('N',{handle});if(!npc) return {};return QuestStartNpc{99,2,9};}
    bool active(std::uint16_t id) override {event('A',{id});return has_active!=0;}
    bool finished(std::uint16_t id) override {event('F',{id});return has_finished!=0;}
    bool find_item(std::uint8_t type,std::uint8_t id,bool consume) override {event('I',{type,id,consume?1u:0u});if(!inventory) return false;if(consume) --inventory;return true;}
    std::uint32_t tick_ms() override {const auto result=now++;event('T',{result});return result;}
    RestoredQuest* allocate() override {event('O');return allocation?&quest:nullptr;}
    void insert_active(RestoredQuest& q) override {check(&q==&quest && q.definition==definition,"Invalid bound insertion");++active_count;}
    void send_database(const std::array<std::uint8_t,10>& packet) override {
        event('B');const auto c=counts();events.insert(events.end(),c.begin(),c.end());events.insert(events.end(),packet.begin(),packet.end());
        QuestReplica replica(billing,pool);check(replica.apply(packet)==InventoryReplicaResult::applied,"DB add rejected");check(replica.active().size()==1,"DB add missing");
    }
    bool add(const QuestStartRecord& record) override {check(&record==&definition->record(),"Positional definition binding changed");return quest_start_add(*definition,user,*this);}
    void prepare_start_log(const QuestStartRecord& q,const std::optional<QuestStartNpc>& n) override {event('P');std::optional<QuestStartLogNpc> context;if(n) context=QuestStartLogNpc{n->type,"NPC"};log=quest_start_log_packet({},q,context);}
    void send_self(const std::array<std::uint8_t,8>& p) override {++notifications;event('C');events.insert(events.end(),p.begin(),p.end());}
    void log_start(const QuestStartRecord&,const std::optional<QuestStartNpc>&) override {event('L');events.insert(events.end(),log.begin(),log.end());}
    bool give_item(std::uint8_t type,std::uint8_t id,std::uint8_t count) override {event('G',{type,id,count});return false;}
};
}
int main(int argc,char** argv) {try {
    check(argc==2,"Expected flow fixture");std::ifstream input(argv[1]);check(input.good(),"Missing flow fixture");unsigned cases=0,starts=0,consumed_failures=0;
    for(std::string line;std::getline(input,line);) {
        if(line.empty() || line[0]=='#') continue;World w;unsigned id,last,started,remaining;std::uint32_t mob,player,deadline,target;std::string block,snapshot,events;
        std::istringstream row(line);row>>id>>block>>w.now>>w.billing>>mob>>player>>deadline>>last>>target>>w.npc>>w.has_active>>w.has_finished>>w.inventory>>w.allocation>>started>>remaining>>snapshot>>events;check(!row.fail(),"Malformed flow fixture "+std::to_string(id));
        const auto definitions=QuestDefinitions::decode(decode(block));w.definition=definitions.find(1);check(w.definition!=nullptr,"Missing canonical definition");w.mob=std::bit_cast<std::int32_t>(mob);w.player=std::bit_cast<std::int32_t>(player);
        QuestStartUser admission{20,0,0,1,static_cast<std::uint8_t>(last),deadline,{}};
        const auto before=w.inventory;const bool result=quest_start_request(admission,std::span(&w.definition->record(),1),1,target,w);
        check(result==(started!=0) && w.notifications==started,"Start return/notification mismatch "+std::to_string(id));
        check(w.snapshot()==decode(snapshot),"Flow state mismatch "+std::to_string(id));check(w.events==decode(events),"Flow event/packet order mismatch "+std::to_string(id));check(w.inventory==remaining,"Item consumption mismatch");
        if(!result && before>w.inventory) ++consumed_failures;if(result) ++starts;++cases;
    }
    check(cases==640 && starts>100 && consumed_failures>0,"Insufficient integrated flow coverage");std::cout<<cases<<" original request/add/start flows; "<<starts<<" starts; "<<consumed_failures<<" consumed-item allocation failures matched\n";
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}}
