#include "eternity/game/zone_mobs.hpp"
#include "eternity/game/mob_pool.hpp"
#include "mob_damage_fixture.hpp"
#include <array>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace eternity::game;
void check(bool b) {if(!b)throw std::runtime_error("Mob global traversal mismatch");}
struct Ports final:ZoneMobServices,MobPoolServices {
    MobDamageFixture damage;unsigned released=0;
    MobDamagePool& damage_pool() override {return damage.pool;}
    std::unique_ptr<MobRuntime> construct() override {return std::make_unique<MobRuntime>();}
    std::uint32_t unix_time() override {return 1;}
    void release_compiled_ai(MobRuntime&) override {++released;}
    void release_stack_ai(MobRuntime&) override {throw std::logic_error("Unexpected stack AI");}
    void close_lua(void*) override {throw std::logic_error("Unexpected Lua");}
    std::uint32_t tick_ms() override {return 1;}
    void send_cell_view(CellCoordinates,const eternity::net::Packet&) override {}
    std::unique_ptr<MobBroadcast> allocate_broadcast(const eternity::net::Packet&,std::optional<std::uint8_t>) override {throw std::logic_error("Unexpected broadcast");}
    void lua_init(MobRuntime&) override {throw std::logic_error("Unexpected Lua init");}
};
std::string join(const std::vector<int>& v){std::string s;for(auto x:v){if(!s.empty())s+=',';s+=std::to_string(x);}return s;}
int main(int argc,char** argv) {try {
    check(argc==2);std::ifstream in(argv[1]);check(in.good());std::string line;unsigned cases=0;
    while(std::getline(in,line)) {
        if(line.empty() || line[0]=='#')continue;
        std::istringstream row(line);std::string op,trace_expected,members_expected;int trigger,target;
        row>>op>>trigger>>target>>trace_expected>>members_expected;check(bool(row));
        Ports ports;std::uint32_t counter=0;MobPool pool(counter,ports);ZoneMobs zone(MapGeometry(81),ports);
        std::array<MobRuntime*,4> mobs{};for(auto& m:mobs){m=pool.allocate();check(m!=nullptr);m->position={1,0,1};}
        for(int i=0;i<3;++i)check(zone.enter(*mobs[i]));
        std::vector<int> trace;bool fired=false;
        const auto id=[&](MobRuntime& m){for(int i=0;i<4;++i)if(mobs[i]==&m)return i;throw std::logic_error("Unknown actor");};
        std::function<void(MobRuntime&)> visitor;
        visitor=[&](MobRuntime& m){
            const auto i=id(m);trace.push_back(i);check(trace.size()<40);
            if(!fired && i==trigger){fired=true;
                if(op=="remove")check(zone.leave(*mobs[target]));
                else if(op=="add")check(zone.enter(*mobs[target]));
                else if(op=="nested")zone.visit_global(visitor);
                else check(op=="none");
                // A removed current subject remains a valid prepared pool object
                // throughout its callback; no reuse is allowed until it returns.
                check(pool.is_prepared(m));check(m.id==static_cast<unsigned>(i+1));
            }
        };
        zone.visit_global(visitor);std::vector<int> members;for(auto* m:zone.all())members.push_back(id(*m));
        check(join(trace)==trace_expected && join(members)==members_expected);
        for(auto* m:zone.all())zone.leave(*m);
        for(auto* m:mobs)pool.release(*m);
        check(ports.released==4);++cases;
    }
    check(cases==11);
    // A non-current actor may be recycled and reentered while current remains
    // prepared; the new membership must be traversed at its new tail position.
    Ports ports;std::uint32_t counter=0;MobPool pool(counter,ports);ZoneMobs zone(MapGeometry(81),ports);
    auto* a=pool.allocate();auto* b=pool.allocate();a->position=b->position={1,0,1};check(zone.enter(*a)&&zone.enter(*b));
    std::vector<unsigned> ids;bool fired=false;
    zone.visit_global([&](MobRuntime& m){ids.push_back(m.id);if(!fired){fired=true;check(&m==a);zone.leave(*b);pool.release(*b);auto* reused=pool.allocate();check(reused==b);reused->position={1,0,1};check(zone.enter(*reused));check(pool.is_prepared(m)&&m.id==1);}});
    check((ids==std::vector<unsigned>{1,3}));zone.leave(*a);zone.leave(*b);pool.release(*a);pool.release(*b);check(ports.released==3);
    std::cout<<cases<<" original cursor traces and retained pool lifetime passed\n";
} catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
