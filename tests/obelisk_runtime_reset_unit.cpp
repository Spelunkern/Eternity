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
    int scenario=0;
    ObeliskRuntimeMap map;
    std::vector<ObeliskRuntime> obelisks;
    std::array<MobRuntime,3> principals;
    std::array<MobRuntime,6> companions;
    std::array<MobRuntimeTemplate,3> definitions;
    std::vector<std::uint32_t> trace;
    void event(std::initializer_list<std::uint32_t> values) {trace.insert(trace.end(),values);}
    [[noreturn]] static void unexpected() {throw std::runtime_error("Unexpected reset world service");}
    std::string_view mob_name(const MobRuntime&) override {return "ABCDEFGHIJKLMNOPQRSTUVWXYZ12345";}
    void remove_by_id(std::uint32_t id) override {
        event({1,id,static_cast<std::uint32_t>(map.reset_send)});
        const auto mode=scenario/9%3;
        if(id>=100 && id<103 && mode) {
            const auto index=static_cast<std::size_t>(id-100);auto& o=obelisks.at(index);
            // Explicit removal-provider projection matching the oracle. This
            // composes the real native Destroy body; it is not a zone provider.
            o.respawn.current=std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(o.respawn.current)-1U);
            o.respawn.next=0x12340000U+id;
            if(mode==2) {o.base_country=(o.base_country+1)%3;o.change_time^=0x55aa55aaU;o.last_destroy_country=-1;}
            destroy_obelisk(map,o,index,principals[index],o.base_country,true,{30,-15},*this);
        }
    }
    void send_change(std::size_t index) override {
        const auto& o=obelisks.at(index);
        event({2,static_cast<std::uint32_t>(index),static_cast<std::uint32_t>(map.reset_send),static_cast<std::uint32_t>(o.country),
               static_cast<std::uint32_t>(o.last_destroy_country),static_cast<std::uint32_t>(o.respawn.current),o.respawn.next});
    }
    void packet(std::uint32_t tag,const Packet& p) {event({tag,static_cast<std::uint32_t>(p.size())});trace.insert(trace.end(),p.begin(),p.end());}
    void game_log(const Packet& p) override {packet(6,p);}
    void database(const Packet& p) override {packet(7,p);}
    void update_portal_links() override {event({8,static_cast<std::uint32_t>(map.reset_send)});}
    void add_kill_count(std::int32_t,std::int32_t) override {unexpected();}
    bool kill_count_status14(std::int32_t) override {unexpected();}
    void subtract_kill_count(std::int32_t,std::int32_t) override {unexpected();}
    void broadcast(const Packet&) override {unexpected();}
    MobRuntime* allocate(bool) override {unexpected();}
    void prepare_companion(MobRuntime&) override {unexpected();}
    void initialize(MobRuntime&,const MobRuntimeTemplate&,MobRespawn&) override {unexpected();}
    void enter(MobRuntime&) override {unexpected();}
    std::int32_t random() override {unexpected();}
};
void run(int scenario,const std::vector<std::uint32_t>& expected) {
    World w;w.scenario=scenario;w.map.zone_present=scenario%17!=0;w.map.reset_send=scenario/5%2!=0;w.map.id=42;
    const auto count=static_cast<std::size_t>(scenario%4);w.obelisks.resize(count);
    const auto now=std::array<std::uint32_t,5>{0,100,0x7fffffffU,0xfff6d83fU,0xffffffffU}[static_cast<std::size_t>(scenario/7%5)];
    for(std::size_t oi=0;oi<count;++oi) {
        auto& o=w.obelisks[oi];o.country=(scenario/4+static_cast<int>(oi))%3;o.base_country=(scenario/12+static_cast<int>(oi))%3;
        o.last_destroy_country=std::array<int,4>{-1,0,1,2}[(static_cast<std::size_t>(scenario/36)+oi)%4];
        o.change_time=0xfedcba98U^static_cast<std::uint32_t>(oi);o.base_id=0x12345678U+static_cast<std::uint32_t>(oi);
        o.respawn.current=std::array<int,4>{-1,0,1,2}[(static_cast<std::size_t>(scenario/11)+oi)%4];o.respawn.next=0xf0000000U+static_cast<std::uint32_t>(oi);
        auto& mob=w.principals[oi];mob.id=100U+static_cast<std::uint32_t>(oi);mob.type=static_cast<std::uint16_t>(654+oi);
        mob.definition=&w.definitions[oi];w.definitions[oi].id=static_cast<std::uint16_t>(321+oi);mob.position={1.25F+static_cast<float>(oi),2.5,3.75};
        o.principal=(static_cast<std::size_t>(scenario/2)+oi)%3?&mob:nullptr;
        o.spawns.resize(1);auto& s=o.spawns[0];s.count=2;s.respawn.current=1;s.respawn.next=2345;s.members.resize(2);
        for(std::size_t n=0;n<2;++n) {
            auto& member=w.companions[oi*2+n];member.id=static_cast<std::uint32_t>(700+oi*2+n);
            s.members[n]=(static_cast<std::size_t>(scenario)+oi+n)%2==0?&member:nullptr;
        }
    }
    reset_obelisks(w.map,w.obelisks,[&] {w.event({15,now});return now;},w);
    w.event({97,static_cast<std::uint32_t>(w.map.reset_send)});
    for(const auto& o:w.obelisks) {
        w.event({99,static_cast<std::uint32_t>(o.country),static_cast<std::uint32_t>(o.last_destroy_country),static_cast<std::uint32_t>(o.base_country),
                 o.change_time,static_cast<std::uint32_t>(o.respawn.current),o.respawn.next,static_cast<std::uint32_t>(o.principal!=nullptr)});
        for(const auto& s:o.spawns) {w.event({98,static_cast<std::uint32_t>(s.respawn.current),s.respawn.next});for(auto* m:s.members) w.event({static_cast<std::uint32_t>(m!=nullptr)});}
    }
    if(w.trace!=expected) {
        std::size_t i=0;while(i<w.trace.size()&&i<expected.size()&&w.trace[i]==expected[i]) ++i;
        std::cerr<<"Scenario "<<scenario<<" divergence at "<<i<<" native="<<(i<w.trace.size()?std::to_string(w.trace[i]):"end")
                 <<" original="<<(i<expected.size()?std::to_string(expected[i]):"end")<<'\n';
        throw std::runtime_error("Original reset comparison failed");
    }
}
}
int main(int argc,char** argv) {
    try {
        if(argc!=2) throw std::runtime_error("Expected reset oracle fixture");
        std::ifstream input(argv[1]);if(!input) throw std::runtime_error("Missing reset oracle fixture");
        std::string line;unsigned count=0;
        while(std::getline(input,line)) {
            if(line.empty()||line[0]=='#') continue;
            std::istringstream row(line);int scenario;row>>scenario;std::vector<std::uint32_t> expected;std::uint32_t word;
            while(row>>word) expected.push_back(word);run(scenario,expected);++count;
        }
        if(count!=648) throw std::runtime_error("Incomplete reset oracle");
        std::cout<<count<<" original reset/nested-destroy scenarios passed\n";return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
