#include "eternity/game/quest_runtime.hpp"
#include "eternity/game/quest_replica.hpp"
#include <algorithm>
#include <bit>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace eternity;using namespace eternity::game;
namespace {
void check(bool value,const std::string& message) {if(!value) throw std::runtime_error(message);}
net::Packet decode(const std::string& text) {net::Packet out;if(text=="-") return out;check(text.size()%2==0,"Invalid quest ownership hex");for(std::size_t i=0;i<text.size();i+=2) out.push_back(static_cast<std::uint8_t>(std::stoul(text.substr(i,2),nullptr,16)));return out;}
void append(net::Packet& out,std::uint32_t value,unsigned n=4) {for(unsigned i=0;i<n;++i) out.push_back(static_cast<std::uint8_t>(value>>(8*i)));}
std::uint32_t field(const net::Packet& bytes,std::size_t offset,unsigned n=4) {std::uint32_t value=0;for(unsigned i=0;i<n;++i) value|=std::uint32_t(bytes.at(offset+i))<<(8*i);return value;}
struct Services final:QuestRuntimePoolServices {
    std::vector<RestoredQuest*> records;bool allocate=true;
    std::unique_ptr<RestoredQuest> construct() override {if(!allocate) return nullptr;auto quest=std::make_unique<RestoredQuest>();records.push_back(quest.get());return quest;}
    std::int32_t identity(const RestoredQuest* quest) const {if(!quest) return -1;const auto at=std::find(records.begin(),records.end(),quest);check(at!=records.end(),"Unknown native quest identity");return static_cast<std::int32_t>(at-records.begin());}
};
struct World final:QuestRuntimeWorld {
    QuestRuntime& runtime;Services& services;QuestReplica& cache;std::uint16_t reentry=0;net::Packet events;
    World(QuestRuntime& owner,Services& heap,QuestReplica& replica):runtime(owner),services(heap),cache(replica) {}
    void send_database(const net::Packet& packet) override {
        events.push_back('B');append(events,static_cast<std::uint32_t>(packet.size()));events.insert(events.end(),packet.begin(),packet.end());
        check(cache.apply(packet)==InventoryReplicaResult::applied,"Real DB quest replica rejected runtime removal");
        if(reentry) {auto* found=runtime.find_active(reentry);events.push_back('F');append(events,static_cast<std::uint32_t>(services.identity(found)));}
    }
};
struct Episode {
    Services services;QuestRuntimePool pool{services};QuestDefinitions definitions;QuestRuntime runtime;
    QuestReplicaPool cache_pool;QuestReplica cache;World world;
    Episode(std::uint32_t account,QuestDefinitions defs):definitions(std::move(defs)),runtime(account,pool),cache(account,cache_pool),world(runtime,services,cache) {}
    unsigned definition_index(const QuestDefinition* pointer) const {if(!pointer) return 0;for(unsigned i=1;i<=definitions.size();++i) if(definitions.find(static_cast<std::uint16_t>(i))==pointer) return i;throw std::runtime_error("Unknown quest definition binding");}
    net::Packet payload(const RestoredQuest& q) const {
        net::Packet out;append(out,q.packet.id,2);append(out,q.success,1);append(out,q.start_tick);append(out,q.packet.end_tick);append(out,q.success_tick);
        out.insert(out.end(),q.packet.counters.begin(),q.packet.counters.end());append(out,definition_index(q.definition),2);return out;
    }
    void load(const net::Packet& payloads) {
        check(payloads.size()==140,"Unexpected seeded quest count");
        for(unsigned i=0;i<7;++i) {
            const auto offset=i*20;RestoredQuest q;q.packet.id=static_cast<std::uint16_t>(field(payloads,offset,2));q.success=payloads[offset+2];
            q.start_tick=field(payloads,offset+3);q.packet.end_tick=field(payloads,offset+7);q.success_tick=field(payloads,offset+11);std::copy_n(payloads.begin()+offset+15,3,q.packet.counters.begin());q.definition=definitions.find(static_cast<std::uint16_t>(field(payloads,offset+18,2)));
            if(i<5) {check(runtime.load_active(q),"Cannot seed active quest");check(cache.load({q.packet.id,0,q.packet.counters}),"Cannot seed active DB quest");}
            else {check(runtime.load_finished(q),"Cannot seed finished quest");check(cache.load_finished({q.packet.id,q.success}),"Cannot seed finished DB quest");}
        }
    }
    std::vector<unsigned> identities(std::span<RestoredQuest* const> records,bool reverse=false) const {std::vector<unsigned> out;for(auto* q:records) out.push_back(static_cast<unsigned>(services.identity(q)));if(reverse) std::reverse(out.begin(),out.end());return out;}
};
std::vector<unsigned> ids(const std::string& text) {std::vector<unsigned> out;if(text=="-") return out;std::istringstream row(text);for(std::string item;std::getline(row,item,',');) out.push_back(static_cast<unsigned>(std::stoul(item)));return out;}
}
int main(int argc,char** argv) {try {
    check(argc==2,"Expected original quest removal oracle");std::ifstream input(argv[1]);check(input.good(),"Cannot read quest removal fixture");
    std::unique_ptr<Episode> state;unsigned episodes=0,removals=0,allocations=0,queries=0,snapshots=0;
    for(std::string line;std::getline(input,line);) {
        if(line.empty() || line[0]=='#') continue;
        std::istringstream row(line);char tag;row>>tag;
        if(tag=='I') {
            unsigned episode;std::uint32_t account,mob,pc;std::string block,payload;row>>episode>>account>>mob>>pc>>block>>payload;
            state=std::make_unique<Episode>(account,QuestDefinitions::decode(decode(block)));state->load(decode(payload));
            { // Admission appends before QuestAdd performs its live accounting.
                Services heap;QuestRuntimePool pool(heap);QuestRuntime owner(account,pool);
                auto* admitted=pool.allocate();check(admitted!=nullptr,"Admission allocation failed");
                admitted->definition=state->definitions.find(1);admitted->packet.id=1;
                owner.mob_kill_quests()=17;owner.player_kill_quests()=23;
                owner.add_active_unaccounted(*admitted);
                check(owner.find_active(1)==admitted && owner.active().size()==1 && owner.mob_kill_quests()==17 && owner.player_kill_quests()==23,"Unaccounted admission changed accounting or ownership");
                bool rejected=false;try {owner.add_active_unaccounted(*admitted);}catch(const std::invalid_argument&) {rejected=true;}
                check(rejected && owner.active().size()==1,"Admission accepted duplicate ownership");
            }
            state->runtime.mob_kill_quests()=std::bit_cast<std::int32_t>(mob);state->runtime.player_kill_quests()=std::bit_cast<std::int32_t>(pc);++episodes;
        } else if(tag=='R') {
            unsigned identity,success,reentry;row>>identity>>success>>reentry;state->world.events.clear();state->world.reentry=static_cast<std::uint16_t>(reentry);
            state->runtime.remove(*state->services.records.at(identity),static_cast<std::uint8_t>(success),state->world);++removals;
        } else if(tag=='F' || tag=='T' || tag=='N') {
            RestoredQuest* found=nullptr;std::int32_t expected;
            if(tag=='F') {unsigned id;row>>id;found=state->runtime.find_active(static_cast<std::uint16_t>(id));state->world.events.clear();}
            else if(tag=='T') found=state->runtime.first_active();else found=state->runtime.next_active();
            row>>expected;check(state->services.identity(found)==expected,"Original active quest cursor/query differs");++queries;
        } else if(tag=='A') {
            unsigned allowed;std::int32_t expected;std::string payload;row>>allowed>>expected>>payload;state->world.events.clear();state->world.reentry=0;state->services.allocate=allowed;
            auto* quest=state->pool.allocate();check(state->services.identity(quest)==expected && (quest?state->payload(*quest):net::Packet{})==decode(payload),"Original shared quest heap reuse changed retained payload");if(quest) state->pool.release(*quest);++allocations;
        } else if(tag=='S') {
            std::string active,finished,free,payload,events;std::int32_t cursor;std::uint32_t mob,pc,attempts;row>>active>>finished>>free>>cursor>>mob>>pc>>attempts>>payload>>events;
            check(state->identities(state->runtime.active())==ids(active) && state->identities(state->runtime.finished())==ids(finished) && state->identities(state->pool.available(),true)==ids(free),"Original quest owner/heap ordering differs at state "+std::to_string(snapshots));
            check(state->services.identity(state->runtime.active_cursor())==cursor && static_cast<std::uint32_t>(state->runtime.mob_kill_quests())==mob && static_cast<std::uint32_t>(state->runtime.player_kill_quests())==pc && state->pool.construction_attempts()==attempts,"Original quest cursor/counters/attempts differ at state "+std::to_string(snapshots));
            net::Packet actual;for(auto* quest:state->services.records) {const auto bytes=state->payload(*quest);actual.insert(actual.end(),bytes.begin(),bytes.end());}
            check(actual==decode(payload) && state->world.events==decode(events),"Original quest payload/delivery/reentrant state differs at state "+std::to_string(snapshots));++snapshots;
        } else throw std::runtime_error("Unknown quest removal fixture row");
        check(static_cast<bool>(row),"Malformed quest ownership fixture");std::string extra;check(!(row>>extra),"Trailing quest ownership fixture data");
    }
    check(episodes==64 && removals==320 && allocations==320 && queries>600 && snapshots>1500,"Quest removal coverage changed");
    std::cout<<episodes<<" original quest owners, "<<removals<<" removals, "<<allocations<<" shared heap reuse operations and "<<snapshots<<" states passed with real DB replica delivery\n";
} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}}
