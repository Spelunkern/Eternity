#include "eternity/game/mob_quests.hpp"
#include "eternity/game/quest_replica.hpp"
#include "mob_damage_fixture.hpp"
#include "mob_death_reward_fixture.hpp"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <iostream>
#include <stdexcept>
using namespace eternity;using namespace eternity::game;
namespace {
void check(bool value,const std::string& message) {if(!value) throw std::runtime_error(message);}
net::Packet decode(const std::string& text) {
    net::Packet bytes;if(text=="-") return bytes;check(text.size()%2==0,"Invalid quest hex");
    for(std::size_t i=0;i<text.size();i+=2) bytes.push_back(static_cast<std::uint8_t>(std::stoul(text.substr(i,2),nullptr,16)));return bytes;
}
void append(net::Packet& bytes,std::uint32_t value,unsigned n=4) {for(unsigned i=0;i<n;++i) bytes.push_back(static_cast<std::uint8_t>(value>>(8*i)));}
std::uint32_t number(const net::Packet& bytes,std::size_t offset) {std::uint32_t value=0;for(unsigned i=0;i<4;++i) value|=std::uint32_t(bytes.at(offset+i))<<(8*i);return value;}
struct World final:MobQuestWorld {
    QuestReplicaPool pool;QuestReplica replica{7,pool};std::array<RestoredQuest,4> quests{};std::array<bool,4> active{true,true,true,true};
    std::uint32_t now=0;bool remove=false;net::Packet events;unsigned clock_calls=0;
    std::uint32_t tick_ms() override {++clock_calls;return now;}
    RestoredQuest* find_quest(std::uint16_t id) override {for(unsigned i=0;i<4;++i) if(active[i] && quests[i].packet.id==id) return &quests[i];return nullptr;}
    void packet(char tag,const net::Packet& p) {events.push_back(static_cast<std::uint8_t>(tag));append(events,static_cast<std::uint32_t>(p.size()));events.insert(events.end(),p.begin(),p.end());}
    void send_database(const net::Packet& p) override {packet('B',p);check(replica.apply(p)==InventoryReplicaResult::applied,"Mob quest DB progress rejected by real replica");}
    void send_self(const net::Packet& p) override {packet('C',p);}
    void complete_quest(RestoredQuest& quest,std::int32_t selection,bool success) override {events.push_back('E');append(events,quest.packet.id);append(events,static_cast<std::uint32_t>(selection));append(events,success?1:0);if(remove) active[static_cast<std::size_t>(&quest-quests.data())]=false;}
};
}
int main(int argc,char** argv) {try {
    check(argc==2,"Expected original mob quest oracle");std::ifstream input(argv[1]);check(input.good(),"Cannot read mob quest oracle");unsigned builds=0,updates=0;
    for(std::string line;std::getline(input,line);) {
        if(line.empty() || line[0]=='#') continue;std::istringstream row(line);char tag;unsigned scenario;row>>tag>>scenario;
        if(tag=='L') {
            int count;unsigned mask;std::string block,expected;row>>count>>mask>>block>>expected;
            const auto definitions=QuestDefinitions::decode(decode(block));storage::ResultSet rows;
            rows.columns={"MobID","Level","Exp","SP","MP","Dex","Wis","Luc","Defense","Magic","HP","Attacktime3","Attackspecial3","AI","Attrib"};
            for(int i=0;i<count;++i) {storage::Row r(15,storage::Cell{"0"});r[0]=std::to_string(i);r[1]=std::to_string((mask>>i)&1);r[10]="1";rows.rows.push_back(std::move(r));}
            const auto mobs=MobRuntimeTemplates::load(rows);const auto links=MobQuestLinks::build(definitions,count,mobs);net::Packet actual;
            for(const auto& ids:links.records()) {append(actual,static_cast<std::uint32_t>(ids.size()));for(auto id:ids) append(actual,id,2);}
            if(actual!=decode(expected)) {for(unsigned i=0;i<links.records().size();++i) {std::cerr<<i<<':';for(auto id:links.records()[i]) std::cerr<<' '<<id;std::cerr<<'\n';}throw std::runtime_error("Original quest index differs in scenario "+std::to_string(scenario));}
            check(!links.find(0) && !links.find(static_cast<std::uint16_t>(count)),"Mob link index accepted invalid ID");++builds;
        } else if(tag=='K') {
            int have;unsigned linked,mob,now,remove;std::string block,initial,final,events;row>>have>>linked>>mob>>now>>remove>>block>>initial>>final>>events;
            const auto definitions=QuestDefinitions::decode(decode(block));const auto bytes=decode(initial);World world;world.now=now;world.remove=remove;
            for(unsigned i=0;i<4;++i) {auto& q=world.quests[i];q.packet.id=static_cast<std::uint16_t>(i+1);q.definition=definitions.find(q.packet.id);q.success_tick=number(bytes,i*7);std::copy_n(bytes.begin()+i*7+4,3,q.packet.counters.begin());check(world.replica.load({q.packet.id,0,q.packet.counters}),"Cannot load mob quest cache");}
            std::vector<std::vector<std::uint16_t>> entries;if(linked) entries.resize(8,std::vector<std::uint16_t>{1,2,1,999,3,4});const MobQuestLinks links(std::move(entries));
            MobDamageFixture heap;MobRuntime subject;MobRuntimeTemplate definition;definition.level=1;definition.experience=10;
            subject.id=77;subject.type=static_cast<std::uint16_t>(mob);subject.definition=&definition;subject.damage=std::make_unique<MobDamageLedger>(heap.pool);subject.damage->add(1,42,100,100,0);
            MobDeathRewardFixture rewards;rewards.user={42,1,0};rewards.reward=[](MobDeathUser&,std::uint32_t source,std::int32_t amount) {check(source==777 && amount==14,"Quest composition changed death experience");};
            rewards.kill=[&](MobDeathUser&,MobRuntime& killed) {update_mob_kill_quests(7,have,killed.type,links,world);};
            const MapGeometry geometry(81);ZoneMobs zone(geometry,rewards);subject.zone=&zone;mob_death(subject,2,777,{},rewards);
            check(rewards.order==std::vector<char>{'V','F','E','D','Q','C'} && heap.pool.available().size()==1,"Mob death/quest/cleanup composition changed");net::Packet state;
            for(unsigned i=0;i<4;++i) {state.push_back(world.active[i]?1:0);append(state,world.quests[i].success_tick);state.insert(state.end(),world.quests[i].packet.counters.begin(),world.quests[i].packet.counters.end());}
            check(state==decode(final) && world.events==decode(events),"Original quest progress/order differs in scenario "+std::to_string(scenario));
            check(world.clock_calls==(have>0 && linked && mob>0 && mob<8?1U:0U),"Quest update read the clock before source guards");
            const auto cached=world.replica.active();check(cached.size()==4,"Mob progress removed a cache record without QuestEnd body");for(unsigned i=0;i<4;++i) check(cached[i].counters==world.quests[i].packet.counters,"Game/DB quest counters diverged");++updates;
        } else if(tag=='A') {
            unsigned count=0;std::string sha;std::size_t bytes=0;std::uint64_t hash=0;row>>count>>sha>>bytes>>hash;
            check(scenario>0 && count>0 && sha.size()==64 && bytes>0 && hash!=0,"Invalid full quest index reference");
        } else throw std::runtime_error("Unknown mob quest fixture row");
        check(static_cast<bool>(row),"Malformed mob quest fixture row");std::string extra;check(!(row>>extra),"Trailing mob quest fixture data");
    }
    check(builds==128 && updates==512,"Mob quest coverage changed");std::cout<<builds<<" original mob quest indices and "<<updates<<" kill updates with real DB replica delivery passed\n";
} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}}
