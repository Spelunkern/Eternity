#include "eternity/game/quest_start.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace eternity::game;
namespace {
void check(bool b,const std::string& s) {if(!b) throw std::runtime_error(s);}
std::vector<std::uint8_t> decode(const std::string& s) {
    std::vector<std::uint8_t> r;if(s=="-") return r;
    for(std::size_t i=0;i<s.size();i+=2) r.push_back(static_cast<std::uint8_t>(std::stoul(s.substr(i,2),nullptr,16)));return r;
}
struct World:QuestStartWorld {
    std::vector<std::uint8_t> events;unsigned notifications=0;unsigned present=0,npc_type=0,npc_id=0,has_active=0,has_finished=0,item=0,allocation=0,now=0;
    void event(char c,std::initializer_list<std::uint32_t> values={}) {events.push_back(static_cast<std::uint8_t>(c));for(auto n:values) for(unsigned i=0;i<4;++i) events.push_back(static_cast<std::uint8_t>(n>>(8*i)));}
    std::optional<QuestStartNpc> find_npc(std::uint32_t n) override {event('N',{n});if(!present) return {};return QuestStartNpc{99,npc_type,npc_id};}
    bool active(std::uint16_t id) override {event('A',{id});return has_active!=0;}
    bool finished(std::uint16_t id) override {event('F',{id});return has_finished!=0;}
    bool find_item(std::uint8_t type,std::uint8_t id,bool consume) override {event('I',{type,id,consume?1u:0u});return item!=0;}
    std::uint32_t tick_ms() override {event('T');return now;}
    bool add(const QuestStartRecord&) override {event('Q');return allocation!=0;}
    void prepare_start_log(const QuestStartRecord&,const std::optional<QuestStartNpc>&) override {event('P');}
    void send_self(const std::array<std::uint8_t,8>& p) override {++notifications;event('C');events.insert(events.end(),p.begin(),p.end());}
    void log_start(const QuestStartRecord&,const std::optional<QuestStartNpc>&) override {event('L');}
    bool give_item(std::uint8_t type,std::uint8_t id,std::uint8_t count) override {event('G',{type,id,count});return false;}
};
}
int main(int argc,char** argv) {try {
    check(argc==2,"Expected oracle fixture");std::ifstream input(argv[1]);check(input.good(),"Cannot read oracle");unsigned cases=0,starts=0;
    const std::array<QuestStartFinished,3> previous{{{7,1},{9,0},{65535,2}}};
    for(std::string line;std::getline(input,line);) {
        if(line.empty() || line[0]=='#') continue;
        std::istringstream row(line);unsigned case_id,level,country,grow,job,last,request,enabled;std::uint32_t deadline,target;std::string hex,expected;World world;
        row>>case_id>>hex>>level>>country>>grow>>job>>last>>deadline>>world.now>>request>>target>>world.present>>world.npc_type>>world.npc_id>>world.has_active>>world.has_finished>>world.item>>world.allocation>>enabled>>expected;
        check(!row.fail(),"Malformed oracle");auto bytes=decode(hex);check(bytes.size()==464,"Invalid record");QuestStartRecord record;std::copy(bytes.begin(),bytes.end(),record.begin());
        QuestStartUser user{static_cast<std::uint16_t>(level),static_cast<std::uint8_t>(country),static_cast<std::uint8_t>(grow),static_cast<std::uint8_t>(job),static_cast<std::uint8_t>(last),deadline,previous};
        check(quest_enable_start(user,record)==(enabled!=0),"Admission mismatch "+std::to_string(case_id));
        const bool started=quest_start_request(user,std::span(&record,1),static_cast<std::uint16_t>(request),target,world);
        check(world.events==decode(expected),"Request trace mismatch "+std::to_string(case_id));
        check(started==(world.notifications==1),"Start return disagrees with client notification");starts+=started?1:0;++cases;
    }
    check(cases==1536 && starts>50,"Insufficient original execution coverage");std::cout<<cases<<" original admission/request traces; "<<starts<<" starts matched\n";
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}}
