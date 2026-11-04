#include "eternity/game/quest_start_add.hpp"
#include "eternity/game/quest_replica.hpp"
#include <algorithm>
#include <bit>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace eternity;using namespace eternity::game;
namespace {
void check(bool b,const std::string& text) {if(!b) throw std::runtime_error(text);}
net::Packet decode(const std::string& s) {net::Packet r;if(s=="-") return r;for(std::size_t i=0;i<s.size();i+=2) r.push_back(static_cast<std::uint8_t>(std::stoul(s.substr(i,2),nullptr,16)));return r;}
void append(net::Packet& p,std::uint32_t v,unsigned n=4) {for(unsigned i=0;i<n;++i) p.push_back(static_cast<std::uint8_t>(v>>(8*i)));}
struct World final:QuestStartAddWorld {
    RestoredQuest quest;const QuestDefinition* definition=nullptr;
    std::uint32_t billing=0,now=0,new_billing=0,new_mob=0,new_player=0,active_count=0;
    std::int32_t mob=0,player=0;bool allocation=false,mutate=false;net::Packet events;std::vector<std::uint32_t> ids;
    QuestStartAddUser user{billing,mob,player};
    net::Packet snapshot() const {net::Packet p;append(p,quest.packet.id);append(p,quest.success,1);for(auto c:quest.packet.counters) append(p,c,1);append(p,quest.start_tick);append(p,quest.packet.end_tick);append(p,quest.success_tick);append(p,quest.definition==definition?1:0);return p;}
    net::Packet counts() const {net::Packet p;append(p,active_count);append(p,static_cast<std::uint32_t>(mob));append(p,static_cast<std::uint32_t>(player));append(p,billing);return p;}
    void event(char c,bool state=false,bool counters=false) {events.push_back(static_cast<std::uint8_t>(c));if(state) {auto p=snapshot();events.insert(events.end(),p.begin(),p.end());}if(counters) {auto p=counts();events.insert(events.end(),p.begin(),p.end());}}
    RestoredQuest* allocate() override {event('A');return allocation?&quest:nullptr;}
    std::uint32_t tick_ms() override {event('T',true);return now;}
    void insert_active(RestoredQuest& q) override {
        check(&q==&quest,"Wrong allocated instance");event('E',true,true);ids.push_back(q.packet.id);++active_count;event('L',true,true);
        if(mutate) {billing=new_billing;mob=std::bit_cast<std::int32_t>(new_mob);player=std::bit_cast<std::int32_t>(new_player);}
    }
    void send_database(const std::array<std::uint8_t,10>& packet) override {
        event('B',false,true);events.insert(events.end(),packet.begin(),packet.end());
        QuestReplicaPool pool;QuestReplica replica(billing,pool);
        check(replica.apply(packet)==InventoryReplicaResult::applied,"Actual quest DB replica rejected add");
        const auto active=replica.active();check(active.size()==1 && active[0].id==definition->id() && active[0].delay==static_cast<std::uint16_t>(definition->maximum_minutes()),"Actual DB cache add mismatch");
    }
};
}
int main(int argc,char** argv) {try {
    check(argc==2,"Expected original add fixture");std::ifstream input(argv[1]);check(input.good(),"Missing fixture");unsigned cases=0,added=0;
    for(std::string line;std::getline(input,line);) {
        if(line.empty() || line[0]=='#') continue;
        std::istringstream row(line);unsigned id,allocation,poison,mutation,result;std::uint32_t mob,player;std::string block,state,counters,ids,events;World w;
        row>>id>>block>>allocation>>w.now>>w.billing>>mob>>player>>w.active_count>>poison>>mutation>>w.new_billing>>w.new_mob>>w.new_player>>result>>state>>counters>>ids>>events;check(!row.fail(),"Malformed fixture");
        const auto definitions=QuestDefinitions::decode(decode(block));w.definition=definitions.find(1);check(w.definition!=nullptr,"Missing definition");w.allocation=allocation!=0;w.mutate=mutation!=0;w.mob=std::bit_cast<std::int32_t>(mob);w.player=std::bit_cast<std::int32_t>(player);
        for(std::uint32_t i=0;i<w.active_count;++i) w.ids.push_back(0xaa00+i);
        const auto retained=std::uint32_t(poison)*0x01010101u;w.quest.packet={0xface,retained,true,{static_cast<std::uint8_t>(poison),static_cast<std::uint8_t>(poison),static_cast<std::uint8_t>(poison)}};w.quest.success=static_cast<std::uint8_t>(poison);w.quest.start_tick=retained;w.quest.success_tick=retained;
        const auto actual=quest_start_add(*w.definition,w.user,w);
        check(actual==(result!=0),"Return mismatch "+std::to_string(id));check(w.snapshot()==decode(state),"State mismatch "+std::to_string(id));check(w.counts()==decode(counters),"Accounting mismatch "+std::to_string(id));check(w.events==decode(events),"Ordering mismatch "+std::to_string(id));
        std::ostringstream list;for(std::size_t i=0;i<w.ids.size();++i) {if(i) list<<',';list<<w.ids[i];}check((w.ids.empty()?"-":list.str())==ids,"List order mismatch");
        if(actual) {check(w.quest.packet.timed==(w.definition->maximum_minutes()!=0),"Timed flag must derive from minutes, even on zero-wrapped end tick");++added;}++cases;
    }
    check(cases==512 && added>400,"Insufficient original add coverage");std::cout<<cases<<" original complete QuestAdd cases; "<<added<<" allocations/DB replica adds matched\n";
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}}
