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
    int scenario=0,allocations=0,random_count=0;std::uint32_t id=500;
    std::vector<std::uint32_t> trace;std::array<MobRuntime,8> mobs;ObeliskRuntime* ob=nullptr;
    void event(std::initializer_list<std::uint32_t> v) {trace.insert(trace.end(),v);}
    std::uint32_t identity(MobRuntime& m) {for(std::size_t i=0;i<mobs.size();++i) if(&m==&mobs[i]) return static_cast<std::uint32_t>(i+100);return 77;}
    std::string_view mob_name(const MobRuntime&) override {return "ABCDEFGHIJKLMNOPQRSTUVWXYZ12345";}
    void remove_by_id(std::uint32_t v) override {event({1,v});}
    void send_change(std::size_t n) override {event({2,static_cast<std::uint32_t>(n)});}
    void add_kill_count(std::int32_t c,std::int32_t n) override {event({3,static_cast<std::uint32_t>(c),static_cast<std::uint32_t>(n)});}
    bool kill_count_status14(std::int32_t) override {return scenario/4%2;}
    void subtract_kill_count(std::int32_t c,std::int32_t n) override {event({4,static_cast<std::uint32_t>(c),static_cast<std::uint32_t>(n)});}
    void packet(std::uint32_t tag,const Packet& p) {event({tag,static_cast<std::uint32_t>(p.size())});trace.insert(trace.end(),p.begin(),p.end());}
    void broadcast(const Packet& p) override {packet(5,p);}
    void game_log(const Packet& p) override {packet(6,p);}
    void database(const Packet& p) override {packet(7,p);}
    void update_portal_links() override {event({8});}
    MobRuntime* allocate(bool principal) override {
        event({9,static_cast<std::uint32_t>(principal)});const int attempt=allocations++;
        if((scenario/9%4==1 && attempt==0)||(scenario/9%4==2 && attempt%2==1)) return nullptr;
        return &mobs.at(static_cast<std::size_t>(attempt));
    }
    void prepare_companion(MobRuntime& m) override {m.id=++id;event({10,id});}
    void initialize(MobRuntime& m,const MobRuntimeTemplate& d,MobRespawn& r) override {
        m.definition=&d;m.type=d.id;m.respawn=&r;
        std::uint32_t offset=64;
        for(std::size_t g=0;g<ob->spawns.size();++g) if(&r==&ob->spawns[g].respawn) offset=static_cast<std::uint32_t>(1048+g*88);
        event({11,identity(m),d.id,offset});
    }
    void enter(MobRuntime& m) override {event({12,identity(m),std::bit_cast<std::uint32_t>(m.position.x),std::bit_cast<std::uint32_t>(m.position.y),std::bit_cast<std::uint32_t>(m.position.z)});}
    std::int32_t random() override {const int v=(random_count++*17+scenario*13)%32768;event({13,static_cast<std::uint32_t>(v)});return v;}
};
void run(int scenario,const std::vector<std::uint32_t>& expected) {
    World w;w.scenario=scenario;ObeliskRuntime o;w.ob=&o;
    o.country=scenario%3;o.last_destroy_country=scenario/27%3;o.base_country=scenario/9%3;o.change_time=0xfedcba98;o.base_id=0x12345678;
    o.center={2.5,3.5,4.5};o.respawn.current=9;o.respawn.next=1234;
    MobRuntimeTemplate definition;definition.id=321;
    for(auto& d:o.definitions) d=scenario%11==0?nullptr:&definition;
    MobRuntime dead;dead.id=77;dead.type=654;dead.definition=&definition;dead.position={1.25,2.5,3.75};o.principal=&dead;
    std::array<MobRuntime,6> old;
    o.spawns.resize(2);
    for(std::size_t g=0;g<2;++g) {
        auto& s=o.spawns[g];s.center={0.25F+static_cast<float>(g)*6,1.5,0.75F+static_cast<float>(g)*6};s.radius=1.5;s.radius_twice=4;s.count=g==0?3:1;
        s.members.resize(static_cast<std::size_t>(s.count));s.respawn.current=8;s.respawn.next=2345;
        for(auto& d:s.definitions) d=(scenario+static_cast<int>(g))%7==0?nullptr:&definition;
        for(std::size_t n=0;n<s.members.size();++n) {old[g*3+n].id=static_cast<std::uint32_t>(700+g*3+n);s.members[n]=n%2==0?&old[g*3+n]:nullptr;}
    }
    std::array<std::uint8_t,8> bitmap{};bitmap.fill(scenario/7%2?0xaa:0);
    ObeliskRuntimeMap map{scenario%17!=0,scenario/2%2!=0,42,8,bitmap};
    if(scenario/216==0) destroy_obelisk(map,o,0,dead,scenario/3%3,scenario/8%2!=0,{scenario/5%2?0:30,scenario/6%2?-15:0},w);
    else respawn_obelisk(map,o,w);
    w.event({99,static_cast<std::uint32_t>(o.country),static_cast<std::uint32_t>(o.last_destroy_country),static_cast<std::uint32_t>(o.respawn.current),o.respawn.next,static_cast<std::uint32_t>(o.principal!=nullptr)});
    for(auto& s:o.spawns) {w.event({98,static_cast<std::uint32_t>(s.respawn.current),s.respawn.next});for(auto* m:s.members) w.event({static_cast<std::uint32_t>(m!=nullptr)});}
    if(w.trace!=expected) {
        std::size_t i=0;while(i<w.trace.size()&&i<expected.size()&&w.trace[i]==expected[i]) ++i;
        std::cerr<<"Scenario "<<scenario<<" divergence at "<<i<<" native="<<(i<w.trace.size()?std::to_string(w.trace[i]):"end")<<" original="<<(i<expected.size()?std::to_string(expected[i]):"end")<<'\n';throw std::runtime_error("Original comparison failed");
    }
}
}
int main(int argc,char** argv) {
    try {if(argc!=2) throw std::runtime_error("Expected oracle path");std::ifstream input(argv[1]);if(!input) throw std::runtime_error("Missing oracle");std::string line;int cases=0;
        while(std::getline(input,line)) {if(line.empty()||line[0]=='#') continue;std::istringstream row(line);int n;row>>n;std::vector<std::uint32_t> expected;std::uint32_t v;while(row>>v) expected.push_back(v);run(n,expected);++cases;}
        if(cases!=432) throw std::runtime_error("Incomplete oracle");
        ObeliskRuntimeSpawn s;MobRuntime a,b;s.count=3;s.members={&a,&a,nullptr};s.remove(&a);if(s.members[0]||s.members[1]!=&a) throw std::runtime_error("Remove must stop at first match");s.add(&b);if(s.members[0]!=&b) throw std::runtime_error("Add must select first empty");s.add(&b);if(s.members[2]!=&b) throw std::runtime_error("Duplicate additions allowed");
        std::cout<<cases<<" original obelisk comparisons and member ownership checks passed\n";return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
