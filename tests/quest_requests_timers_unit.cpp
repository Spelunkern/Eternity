#include "eternity/game/quest_requests_timers.hpp"
#include "eternity/game/quest_replica.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace eternity::game;
namespace {
void check(bool ok,const char* text) {if(!ok) throw std::runtime_error(text);}
std::vector<std::uint8_t> unhex(const std::string& s) {std::vector<std::uint8_t> r;if(s=="-") return r;for(std::size_t i=0;i<s.size();i+=2) r.push_back(static_cast<std::uint8_t>(std::stoul(s.substr(i,2),nullptr,16)));return r;}
void append(std::vector<std::uint8_t>& p,std::uint32_t n,unsigned bytes=4) {for(unsigned i=0;i<bytes;++i) p.push_back(static_cast<std::uint8_t>(n>>(8*i)));}
std::uint32_t get(std::span<const std::uint8_t> p,std::size_t offset) {std::uint32_t n=0;for(unsigned i=0;i<4;++i) n|=static_cast<std::uint32_t>(p[offset+i])<<(8*i);return n;}
std::vector<unsigned> sequence(const std::string& text) {std::vector<unsigned> r;if(text=="-") return r;std::istringstream s(text);for(std::string id;std::getline(s,id,',');) r.push_back(static_cast<unsigned>(std::stoul(id)));return r;}
// Explicit observed QuestEnd(false)/owner test port. It is not a production
// QuestEnd implementation. The whole original core runs in the oracle fixture.
struct World final:QuestRequestsTimerWorld {
    QuestReplicaPool replica_pool;QuestReplica replica{7,replica_pool};
    const QuestDefinitions& definitions;std::array<RestoredQuest,6> quests{};
    std::vector<unsigned> active,finished,free,external;std::vector<std::uint8_t> events;
    int cursor=-1;unsigned mode=0,depth=0,expirations=0;
    explicit World(const QuestDefinitions& d):definitions(d) {}
    void event(char c,std::initializer_list<std::uint32_t> fields={}) {events.push_back(static_cast<std::uint8_t>(c));for(auto f:fields) append(events,f);}
    void lock(unsigned kind) {events.insert(events.end(),{'L',static_cast<std::uint8_t>(kind)});if(kind==0) ++depth;}
    void unlock(unsigned kind) {events.insert(events.end(),{'U',static_cast<std::uint8_t>(kind)});if(kind==0) --depth;}
    void lock_active_quests() override {lock(0);}
    void unlock_active_quests() noexcept override {unlock(0);}
    RestoredQuest* first_active_quest() override {check(depth>0,"unlocked timer cursor");cursor=active.empty()?-1:static_cast<int>(active.front()-1);return cursor<0?nullptr:&quests[static_cast<unsigned>(cursor)];}
    RestoredQuest* next_active_quest() override {
        check(depth>0,"unlocked timer cursor");
        if(cursor<0) return first_active_quest();
        const auto at=std::find(active.begin(),active.end(),static_cast<unsigned>(cursor+1));
        check(at!=active.end(),"cursor lost its active owner");
        cursor=std::next(at)==active.end()?-1:static_cast<int>(*std::next(at)-1);
        return cursor<0?nullptr:&quests[static_cast<unsigned>(cursor)];
    }
    void unlink(unsigned id) {
        const auto at=std::find(active.begin(),active.end(),id);check(at!=active.end(),"duplicate timer removal");
        if(cursor==static_cast<int>(id-1)) cursor=at==active.begin()?-1:static_cast<int>(*std::prev(at)-1);
        active.erase(at);
    }
    void end_quest(RestoredQuest& q,std::uint32_t npc,bool success) override {
        check(depth==1 && npc==0 && !success,"invalid timer QuestEnd boundary");++expirations;
        const auto id=q.packet.id;event('E',{id,npc,success?1U:0U});
        // Core's 0903 failure notice precedes removal and its DB packet.
        std::vector<std::uint8_t> client{3,9};append(client,npc);append(client,id,2);client.resize(22,0);
        events.push_back('C');events.insert(events.end(),client.begin(),client.end());
        if(mode==1) {
            const auto future=std::find_if(active.begin(),active.end(),[&](auto value){return value>id;});
            if(future!=active.end()) {auto& next=quests[*future-1];next.packet.end_tick=0;next.definition=definitions.find(static_cast<std::uint16_t>(*future+6));event('M',{*future});}
        }
        if(mode==4 && !active.empty() && active.back()!=id) {const auto last=active.back();unlink(last);external.push_back(last);event('X',{last});}
        event('R',{id,0});lock(0);unlink(id);unlock(0);
        const auto& d=q.definition->record();const bool repeat=get(d,0x134)!=0;q.success=repeat?2:0;
        if(repeat) {event('F',{id});lock(2);free.insert(free.begin(),id);unlock(2);}
        else {lock(1);finished.push_back(id);unlock(1);}
        std::vector<std::uint8_t> database{2,10};append(database,7);append(database,id,2);database.push_back(q.success);
        check(replica.apply(database)==InventoryReplicaResult::applied,"timer removal rejected by QuestReplica");
        events.push_back('B');events.insert(events.end(),database.begin(),database.end());
        if(mode==2 || mode==3) {cursor=active.empty()?-1:static_cast<int>((mode==2?active.front():active.back())-1);event('J',{static_cast<std::uint32_t>(cursor+1)});}
        events.insert(events.end(),{'G',2,5,static_cast<std::uint8_t>(id),0});
    }
};
}
int main(int argc,char** argv) {try {
    check(argc==2,"fixture required");std::ifstream input(argv[1]);check(input.good(),"cannot read timer fixture");unsigned rows=0,expired=0;
    for(std::string line;std::getline(input,line);) {
        if(line.empty() || line[0]=='#') continue;
        std::istringstream row(line);unsigned scenario,now,mode,count,cursor;std::string block,deadlines,active,finished,free,final_deadlines,final_definitions,events;
        row>>scenario>>now>>mode>>count>>block>>deadlines>>active>>finished>>free>>cursor>>final_deadlines>>final_definitions>>events;check(!row.fail(),"malformed timer fixture");
        const auto definitions=QuestDefinitions::decode(unhex(block));World world(definitions);world.mode=mode;const auto initial=unhex(deadlines);
        for(unsigned i=0;i<6;++i) {
            auto& q=world.quests[i];q.definition=definitions.find(static_cast<std::uint16_t>(i+1));q.packet.id=static_cast<std::uint16_t>(i+1);q.packet.end_tick=get(initial,4*i);
            q.packet.timed=q.definition->maximum_minutes()==0; // Deliberately opposite: UpdateQuest reads the definition field.
            if(i<count) {world.active.push_back(i+1);check(world.replica.load({q.packet.id,0,{}}),"cannot load timer replica");}
        }
        quest_requests_update_timers(now,world);
        std::vector<std::uint8_t> actual_deadlines,actual_definitions;
        for(unsigned i=0;i<6;++i) {const auto& q=world.quests[i];append(actual_deadlines,q.packet.end_tick);unsigned d=0;while(definitions.find(static_cast<std::uint16_t>(d+1))!=q.definition && d<12) ++d;actual_definitions.push_back(static_cast<std::uint8_t>(d));}
        if(world.active!=sequence(active) || world.finished!=sequence(finished) || world.free!=sequence(free) || world.cursor+1!=static_cast<int>(cursor) || actual_deadlines!=unhex(final_deadlines) || actual_definitions!=unhex(final_definitions) || world.events!=unhex(events)) {
            std::cerr<<"timer scenario "<<scenario<<" active "<<(world.active==sequence(active))<<" finished "<<(world.finished==sequence(finished))<<" free "<<(world.free==sequence(free))<<" cursor "<<world.cursor+1<<'/'<<cursor<<" events "<<(world.events==unhex(events))<<'\n';throw std::runtime_error("original timer lifecycle mismatch");
        }
        check(world.depth==0,"timer lock leaked");
        const auto cached=world.replica.active(),saved=world.replica.finished();check(saved.size()==world.finished.size(),"timer DB finished list differs");
        for(auto id:world.active) check(std::any_of(cached.begin(),cached.end(),[&](const auto& q){return q.id==id;}),"timer removed an unexpired DB quest");
        check(cached.size()==world.active.size()+world.external.size(),"timer DB removal count differs");
        for(auto id:world.finished) check(std::any_of(saved.begin(),saved.end(),[&](const auto& q){return q.id==id && q.success==0;}),"timer failure changed saved success");
        expired+=world.expirations;++rows;
    }
    check(rows==512 && expired>100,"insufficient original timer coverage");std::cout<<rows<<" complete original timer lifecycles; "<<expired<<" QuestEnd failures; shared cursor, removal, callback and DB replica traces matched\n";
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}}
