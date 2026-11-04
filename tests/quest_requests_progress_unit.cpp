#include "eternity/game/quest_requests_progress.hpp"
#include "eternity/game/quest_replica.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace eternity::game;
namespace {
void check(bool ok,const char* message) {if(!ok) throw std::runtime_error(message);}
std::vector<std::uint8_t> unhex(const std::string& s) {
    std::vector<std::uint8_t> result;if(s=="-") return result;
    for(std::size_t i=0;i<s.size();i+=2) result.push_back(static_cast<std::uint8_t>(std::stoul(s.substr(i,2),nullptr,16)));
    return result;
}
std::uint32_t number(std::span<const std::uint8_t> p,std::size_t offset,unsigned n=4) {
    std::uint32_t v=0;for(unsigned i=0;i<n;++i) v|=static_cast<std::uint32_t>(p[offset+i])<<(8*i);return v;
}
void append(std::vector<std::uint8_t>& p,std::uint32_t v,unsigned n=4) {for(unsigned i=0;i<n;++i) p.push_back(static_cast<std::uint8_t>(v>>(8*i)));}
struct World final:QuestRequestsProgressWorld {
    QuestReplicaPool pool;QuestReplica replica{7,pool};
    const QuestDefinitions& definitions;
    std::array<RestoredQuest,4> quests{};
    std::array<bool,4> active{true,true,true,true};
    std::array<std::array<std::uint8_t,3>,4> cached{};
    std::vector<std::uint8_t> events;
    int cursor=-1;unsigned mode=0,depth=0,updates=0,queries=0,completions=0;
    bool query=false,remove=false;std::uint32_t now=0;
    explicit World(const QuestDefinitions& d):definitions(d) {}
    void lock_active_quests() override {++depth;events.push_back('L');}
    void unlock_active_quests() noexcept override {--depth;events.push_back('U');}
    RestoredQuest* first_active_quest() override {check(depth>0,"cursor without lock");cursor=-1;return next_active_quest();}
    RestoredQuest* next_active_quest() override {
        check(depth>0,"cursor without lock");
        for(int i=cursor+1;i<4;++i) if(active[static_cast<std::size_t>(i)]) {cursor=i;return &quests[static_cast<std::size_t>(i)];}
        cursor=-1;return nullptr;
    }
    RestoredQuest* find_quest(std::uint16_t id) override {
        lock_active_quests();auto* q=first_active_quest();
        while(q && q->packet.id!=id) q=next_active_quest();
        unlock_active_quests();return q;
    }
    std::uint32_t tick_ms() override {events.push_back('T');return now;}
    void send_database(const eternity::net::Packet& p) override {
        check(p.size()==10 && p[0]==5 && p[1]==10,"unexpected DB progress packet");
        events.push_back('B');events.insert(events.end(),p.begin(),p.end());
        check(replica.apply(p)==InventoryReplicaResult::applied,"progress rejected by real QuestReplica");
        const auto i=number(p,6,2)-1;const auto slot=p[8];cached.at(i).at(slot)=p[9];++updates;
        if(mode==1) quests.at(i).packet.counters.at(slot)=static_cast<std::uint8_t>(p[9]+3);
    }
    void send_self(const eternity::net::Packet& p) override {
        check(p.size()==6 && p[0]==5 && p[1]==9,"unexpected client progress packet");
        events.push_back('C');events.insert(events.end(),p.begin(),p.end());
        if(mode==2) {const auto i=number(p,2,2)-1;quests.at(i).definition=definitions.find(static_cast<std::uint16_t>(i+5));}
    }
    bool find_required_items(RestoredQuest& q,bool consume) override {
        check(consume,"auto completion failed to request consumption");++queries;
        events.push_back('I');append(events,q.packet.id);append(events,consume?1:0);
        if(mode==3) q.packet.counters={2,3,4};
        return query;
    }
    void complete_quest(RestoredQuest& q,std::uint32_t npc,bool success) override {
        check(npc==0 && success,"incorrect completion arguments");++completions;
        events.push_back('E');append(events,q.packet.id);append(events,npc);append(events,success?1:0);
        const auto i=static_cast<std::size_t>(&q-quests.data());
        if(remove && active[i]) {
            active[i]=false;
            if(cursor==static_cast<int>(i)) {cursor=-1;for(int j=static_cast<int>(i)-1;j>=0;--j) if(active[static_cast<std::size_t>(j)]) {cursor=j;break;}}
        }
        if(mode==4) {q.definition=definitions.find(static_cast<std::uint16_t>(i+5));q.packet.counters[2]=254;}
    }
};
ItemQuestLinks links_for(int type,int id,const std::string& text) {
    if(type<=0 || type>=256 || id<=0 || id>=256 || text=="-") return {};
    std::vector<std::uint8_t> words;std::istringstream ids(text);
    for(std::string s;std::getline(ids,s,',');) append(words,static_cast<std::uint32_t>(std::stoul(s)),2);
    std::vector<std::uint8_t> bytes(65536*8,0);const auto offset=static_cast<std::size_t>(type*256+id)*8;
    bytes[offset+4]=static_cast<std::uint8_t>(words.size()/2);bytes.insert(bytes.begin()+static_cast<std::ptrdiff_t>(offset+8),words.begin(),words.end());
    return ItemQuestLinks::decode(bytes);
}
}
int main(int argc,char** argv) {try {
    check(argc==2,"fixture required");std::ifstream input(argv[1]);check(input.good(),"cannot read fixture");
    unsigned pc=0,item=0,updates=0,queries=0,completions=0;
    for(std::string line;std::getline(input,line);) {
        if(line.empty() || line[0]=='#') continue;
        std::istringstream row(line);char kind;unsigned scenario,now,mode,query,remove;int have,type,id,cursor;std::string linked,block,initial,final,expected_events;
        row>>kind>>scenario>>have>>type>>id>>now>>mode>>query>>remove>>linked>>block>>initial>>final>>cursor>>expected_events;check(!row.fail(),"malformed fixture");
        const auto definitions=QuestDefinitions::decode(unhex(block));World world(definitions);world.now=now;world.mode=mode;world.query=query!=0;world.remove=remove!=0;
        const auto bytes=unhex(initial);
        for(unsigned i=0;i<4;++i) {
            auto& q=world.quests[i];q.packet.id=static_cast<std::uint16_t>(i+1);q.definition=definitions.find(q.packet.id);q.success_tick=number(bytes,7*i);
            std::copy_n(bytes.begin()+7*i+4,3,q.packet.counters.begin());world.cached[i]=q.packet.counters;
            check(world.replica.load({q.packet.id,0,q.packet.counters}),"cannot load quest replica");
        }
        if(kind=='P') {quest_requests_progress_kill_pc(7,have,world);++pc;}
        else {check(kind=='I',"unknown progress kind");const auto links=links_for(type,id,linked);quest_requests_progress_pick_item(7,type,id,links,world);++item;}
        std::vector<std::uint8_t> actual;
        for(unsigned i=0;i<4;++i) {
            const auto& q=world.quests[i];actual.push_back(world.active[i]?1:0);
            unsigned definition_index=0;while(definitions.find(static_cast<std::uint16_t>(definition_index+1))!=q.definition && definition_index<8) ++definition_index;
            actual.push_back(static_cast<std::uint8_t>(definition_index));append(actual,q.success_tick);actual.insert(actual.end(),q.packet.counters.begin(),q.packet.counters.end());
        }
        if(actual!=unhex(final) || world.events!=unhex(expected_events) || world.cursor!=cursor) {
            std::cerr<<kind<<" scenario "<<scenario<<" state "<<(actual==unhex(final))<<" events "<<(world.events==unhex(expected_events))<<" cursor "<<world.cursor<<'/'<<cursor<<'\n';throw std::runtime_error("original quest progress mismatch");
        }
        check(world.depth==0,"active list lock leaked");const auto cached=world.replica.active();check(cached.size()==4,"unexpected replica removal without core QuestEnd");
        for(unsigned i=0;i<4;++i) check(cached[i].counters==world.cached[i],"DB counter packet replay mismatch");
        updates+=world.updates;queries+=world.queries;completions+=world.completions;
    }
    check(pc==512 && item==512 && updates>100 && queries>50 && completions>50,"insufficient original progress coverage");
    std::cout<<pc<<" player-kill and "<<item<<" item-pick original progress cases; "<<updates<<" DB/client updates; "<<queries<<" inventory queries; "<<completions<<" completions; callback/unlink/cursor and QuestReplica matched\n";
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}}
