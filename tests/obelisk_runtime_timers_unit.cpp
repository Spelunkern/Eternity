#include "eternity/game/obelisk_runtime.hpp"
#include <bit>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

using namespace eternity::game;
using eternity::net::Packet;
namespace {
struct World final:ObeliskRuntimeWorld {
    bool initialization=false;
    int scenario=0,allocations=0,random_count=0;std::uint32_t id=500;
    std::vector<std::uint32_t> trace;
    std::array<MobRuntime,64> mobs;
    std::array<ObeliskRuntime,2> obelisks;
    std::array<MobRuntimeTemplate,5> definitions;
    void event(std::initializer_list<std::uint32_t> values) {trace.insert(trace.end(),values);}
    std::uint32_t identity(const MobRuntime* mob) const {
        if(!mob) return 0;
        for(std::size_t i=0;i<mobs.size();++i) if(mob==&mobs[i]) return static_cast<std::uint32_t>(i+100);
        throw std::runtime_error("Unknown mob identity");
    }
    [[noreturn]] static void unexpected() {throw std::runtime_error("Unexpected destruction service during respawn update");}
    std::string_view mob_name(const MobRuntime&) override {return "ABCDEFGHIJKLMNOPQRSTUVWXYZ12345";}
    void remove_by_id(std::uint32_t) override {unexpected();}
    void send_change(std::size_t) override {unexpected();}
    void add_kill_count(std::int32_t,std::int32_t) override {unexpected();}
    bool kill_count_status14(std::int32_t) override {unexpected();}
    void subtract_kill_count(std::int32_t,std::int32_t) override {unexpected();}
    void broadcast(const Packet&) override {unexpected();}
    void database(const Packet&) override {unexpected();}
    void update_portal_links() override {unexpected();}
    void game_log(const Packet& p) override {
        event({6,static_cast<std::uint32_t>(p.size())});trace.insert(trace.end(),p.begin(),p.end());
    }
    MobRuntime* allocate(bool manager_create) override {
        event({9,static_cast<std::uint32_t>(manager_create)});const auto attempt=allocations++;
        if((scenario/17%3==1 && attempt%2==0)||scenario/17%3==2) return nullptr;
        return &mobs.at(static_cast<std::size_t>(attempt));
    }
    void prepare_companion(MobRuntime& mob) override {mob.id=++id;event({10,id});}
    void initialize(MobRuntime& mob,const MobRuntimeTemplate& definition,MobRespawn& respawn) override {
        mob.definition=&definition;mob.type=definition.id;mob.respawn=&respawn;
        bool found=false,companion=false;std::uint32_t offset=0;
        for(std::size_t oi=0;oi<obelisks.size();++oi) {
            auto& o=obelisks[oi];
            if(&respawn==&o.respawn) {offset=static_cast<std::uint32_t>(64+oi*112);found=true;}
            for(std::size_t g=0;g<o.spawns.size();++g) if(&respawn==&o.spawns[g].respawn) {
                offset=static_cast<std::uint32_t>(1048+oi*256+g*88);found=true;companion=true;
            }
        }
        if(!found) throw std::runtime_error("Unknown respawn owner");
        event({11,identity(&mob),definition.id,offset});
        if(scenario%11==0 && companion) mob.definition=&definitions[4];
    }
    void enter(MobRuntime& mob) override {
        event({12,identity(&mob),std::bit_cast<std::uint32_t>(mob.position.x),std::bit_cast<std::uint32_t>(mob.position.y),
               std::bit_cast<std::uint32_t>(mob.position.z),static_cast<std::uint32_t>(mob.respawn->current),mob.respawn->next});
        if(initialization && scenario%5==0 && mob.respawn->obelisk_source)
            mob.respawn->current=std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(mob.respawn->current)-1U);
    }
    std::int32_t random() override {
        const auto value=(random_count++*17+scenario*13)%32768;event({13,static_cast<std::uint32_t>(value)});return value;
    }
    void snapshot() {
        for(const auto& o:obelisks) {
            event({99,static_cast<std::uint32_t>(o.respawn.current),o.respawn.next,identity(o.principal)});
            for(const auto& s:o.spawns) {
                event({98,static_cast<std::uint32_t>(s.respawn.current),s.respawn.next});
                for(auto* member:s.members) event({identity(member)});
            }
        }
    }
};
void run(int scenario,const std::vector<std::uint32_t>& expected,bool initialization) {
    World w;w.scenario=scenario;w.initialization=initialization;
    const auto now=std::array<std::uint32_t,7>{0,1,100,0x7fffffffU,0x80000000U,0xfffffffeU,0xffffffffU}[static_cast<std::size_t>(scenario%7)];
    const std::array<std::uint32_t,6> deadlines{0,now-1U,now,now+1U,0xffffffffU,0x80000000U};
    for(std::size_t d=0;d<w.definitions.size();++d) {
        auto& definition=w.definitions[d];definition.id=std::array<std::uint16_t,5>{321,322,401,402,999}[d];
        definition.respawn_time=d==4?0xfffffffcU:std::array<std::uint32_t,4>{0,1,20,0xffffffffU}[(static_cast<std::size_t>(scenario/13)+d)%4];
    }
    for(std::size_t oi=0;oi<w.obelisks.size();++oi) {
        auto& o=w.obelisks[oi];o.country=(scenario+static_cast<int>(oi))%3;o.center={2.5F+static_cast<float>(oi),3.5,4.5};
        const auto country=static_cast<std::size_t>(o.country);
        o.definitions[country]=(scenario+static_cast<int>(oi))%9==0?nullptr:&w.definitions[oi];
        o.respawn.current=std::array<int,4>{-1,0,1,2}[(static_cast<std::size_t>(scenario/7)+oi)%4];
        o.respawn.next=deadlines[(static_cast<std::size_t>(scenario/3)+oi)%6];o.spawns.resize(2);
        for(std::size_t g=0;g<o.spawns.size();++g) {
            auto& s=o.spawns[g];s.center={0.25F+static_cast<float>(g)*6,1.5F+static_cast<float>(oi),0.75F+static_cast<float>(g)*6};
            s.radius=1.5;s.radius_twice=4;s.count=g==0?3:1;s.members.resize(static_cast<std::size_t>(s.count));
            s.definitions[country]=(scenario/2+static_cast<int>(oi)*3+static_cast<int>(g))%5==0?nullptr:&w.definitions[2+g];
            s.respawn.current=std::array<int,4>{-1,0,1,3}[(static_cast<std::size_t>(scenario/11)+oi*2+g)%4];
            s.respawn.next=deadlines[(static_cast<std::size_t>(scenario/5)+oi*2+g)%6];s.respawn.obelisk_source=&s;
        }
    }
    std::array<std::uint8_t,8> bitmap{};bitmap.fill(scenario/3%2?0xaa:0);
    ObeliskRuntimeMap map{scenario%19!=0,false,42,8,bitmap};
    if(initialization) {w.event({16});initialize_obelisks(map,w.obelisks,w);w.snapshot();}
    for(auto tick:{now,now+1U,now+20U}) {w.event({14,tick});update_obelisk_respawns(map,w.obelisks,tick,w);w.snapshot();}
    if(w.trace!=expected) {
        std::size_t i=0;while(i<w.trace.size()&&i<expected.size()&&w.trace[i]==expected[i]) ++i;
        std::cerr<<"Scenario "<<scenario<<" divergence at "<<i<<" native="<<(i<w.trace.size()?std::to_string(w.trace[i]):"end")
                 <<" original="<<(i<expected.size()?std::to_string(expected[i]):"end")<<'\n';
        throw std::runtime_error("Original scheduler comparison failed");
    }
}
}
int main(int argc,char** argv) {
    try {
        if(argc!=2 && argc!=3) throw std::runtime_error("Expected oracle fixture and optional initialization mode");
        const bool initialization=argc==3 && std::string_view(argv[2])=="initialize";
        std::ifstream input(argv[1]);if(!input) throw std::runtime_error("Missing oracle fixture");
        std::string line;unsigned count=0;
        while(std::getline(input,line)) {
            if(line.empty()||line[0]=='#') continue;
            std::istringstream row(line);int scenario;row>>scenario;std::vector<std::uint32_t> expected;
            std::uint32_t word;while(row>>word) expected.push_back(word);run(scenario,expected,initialization);++count;
        }
        if(count!=768) throw std::runtime_error("Incomplete original fixture");
        std::cout<<count<<(initialization?" original startup scenarios and ":" original scheduler scenarios / ")<<count*3<<" stateful tick comparisons passed\n";return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
