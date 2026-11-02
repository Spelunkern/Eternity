#include "eternity/game/zones.hpp"
#include <algorithm>
#include <bit>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <stdexcept>
using namespace eternity::game;using Bytes=eternity::net::Packet;
void require(bool v,const char* message) {if(!v) throw std::runtime_error(message);}
void number(Bytes& b,std::uint32_t v,unsigned int n=4) {for(unsigned int i=0;i<n;++i) b.push_back(static_cast<std::uint8_t>(v>>(8*i)));}
Bytes unhex(const std::string& text) {Bytes b;for(std::size_t i=0;i<text.size();i+=2) b.push_back(static_cast<std::uint8_t>(std::stoul(text.substr(i,2),nullptr,16)));return b;}
struct Services final:MobPoolServices,ZoneMobServices {
    std::array<RuntimeZone,2>* zones=nullptr;std::vector<MobRuntime*> objects,free;Bytes events;bool fail=false,constructing=false;std::uint32_t time=0;int cells=0;
    unsigned int id(const MobRuntime* m) const {if(!m) return 255;const auto at=std::find(objects.begin(),objects.end(),m);require(at!=objects.end(),"Unknown release object");return static_cast<unsigned int>(at-objects.begin());}
    unsigned int zid(const ZoneMobs* zone) const {if(!zone) return 255;for(unsigned int i=0;i<2;++i) if((*zones)[i].mobs.get()==zone) return i;throw std::runtime_error("Unknown raw zone binding");}
    unsigned int index_size(const ZoneMobs& zone) const {std::set<std::uint32_t> keys;for(auto* m:objects) if(zone.find(m->id)) keys.insert(m->id);return static_cast<unsigned int>(keys.size());}
    unsigned int cell_size(const ZoneMobs& zone) const {unsigned int total=0;for(int z=0;z<cells;++z) for(int x=0;x<cells;++x) total+=static_cast<unsigned int>(zone.in_cell({x,z}).size());return total;}
    void observer(char tag,MobRuntime& m) {
        events.push_back(static_cast<std::uint8_t>(tag));number(events,id(&m),1);
        for(auto& z:*zones) {number(events,static_cast<std::uint32_t>(z.mobs->size()));number(events,index_size(*z.mobs));number(events,cell_size(*z.mobs));number(events,static_cast<std::uint32_t>(z.instance_portals[0].id));}
        number(events,zid(m.zone),1);number(events,m.cell?static_cast<std::uint32_t>(m.cell->x):0);number(events,m.cell?static_cast<std::uint32_t>(m.cell->z):0);number(events,static_cast<std::uint32_t>(m.lua_ai));number(events,static_cast<std::uint32_t>(m.only_lua_attack));number(events,static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(m.lua_state)));number(events,m.state_stack?1:0);
    }
    std::unique_ptr<MobRuntime> construct() override {events.push_back('N');number(events,fail?0:1,1);if(fail) return {};auto m=std::make_unique<MobRuntime>();constructing=true;m->drop_random=ServerRandom(unix_time());constructing=false;objects.push_back(m.get());return m;}
    std::uint32_t unix_time() override {if(!constructing) events.push_back('I');events.push_back('T');return time;}
    void release_compiled_ai(MobRuntime& m) override {observer('C',m);free.push_back(&m);}
    void release_stack_ai(MobRuntime& m) override {observer('S',m);}
    void close_lua(void* handle) override {const auto raw=reinterpret_cast<std::uintptr_t>(handle);require(raw>=0x70000000U && (raw-0x70000000U)%16==0,"Unexpected Lua handle");observer('L',*objects.at((raw-0x70000000U)/16));}
    std::uint32_t tick_ms() override {throw std::runtime_error("Unexpected release clock");}
    void send_cell_view(CellCoordinates,const Bytes& packet) override {require(entering_zone<2,"No entry owner");events.push_back('V');number(events,entering_zone,1);number(events,static_cast<std::uint32_t>(packet.size()));events.insert(events.end(),packet.begin(),packet.end());}
    std::unique_ptr<MobBroadcast> allocate_broadcast(const Bytes&,std::optional<std::uint8_t>) override {throw std::runtime_error("Unexpected release movement");}
    void lua_init(MobRuntime& m) override {events.push_back('Q');number(events,id(&m),1);}
    unsigned int entering_zone=255;
};
struct State {
    LinkedMobPopulation linked{std::span<const LinkedMobGroup>{}};MicrosoftRandom random;std::uint32_t counter=0;Services services;std::unique_ptr<MobPool> pool;std::array<RuntimeZone,2> zones;
    void initialize(unsigned int scenario,int extent,std::uint32_t serial) {
        counter=serial;services.zones=&zones;services.cells=extent/40+1;pool=std::make_unique<MobPool>(counter,services);
        for(unsigned int z=0;z<2;++z) {zones[z].mobs=std::make_unique<ZoneMobs>(MapGeometry(extent),services);for(unsigned int i=0;i<10;++i) zones[z].instance_portals[i]={static_cast<std::int32_t>(scenario*100+z*20+i*2+1),static_cast<std::int32_t>(scenario*100+z*20+i*2+2)};}
    }
    Bytes snapshot() {
        Bytes b;number(b,counter);number(b,pool->construction_attempts());number(b,static_cast<std::uint32_t>(pool->available()));number(b,static_cast<std::uint32_t>(services.free.size()),1);for(auto at=services.free.rbegin();at!=services.free.rend();++at) number(b,services.id(*at),1);
        for(auto& zone:zones) {for(auto p:zone.instance_portals) {number(b,static_cast<std::uint32_t>(p.id));number(b,static_cast<std::uint32_t>(p.country));}const auto order=zone.mobs->all();number(b,static_cast<std::uint32_t>(order.size()),1);for(auto* m:order) number(b,services.id(m),1);number(b,services.index_size(*zone.mobs));for(int z=0;z<services.cells;++z) for(int x=0;x<services.cells;++x) number(b,static_cast<std::uint32_t>(zone.mobs->in_cell({x,z}).size()));}
        number(b,static_cast<std::uint32_t>(services.objects.size()),1);
        for(auto* m:services.objects) {number(b,services.zid(m->zone),1);number(b,m->cell?static_cast<std::uint32_t>(m->cell->x):0);number(b,m->cell?static_cast<std::uint32_t>(m->cell->z):0);for(auto f:{m->position.x,m->position.y,m->position.z,m->respawn_position.x,m->respawn_position.y,m->respawn_position.z}) number(b,std::bit_cast<std::uint32_t>(f));number(b,m->id);number(b,static_cast<std::uint32_t>(m->status));number(b,static_cast<std::uint32_t>(m->target_kind));number(b,m->state_stack?1:0);number(b,static_cast<std::uint32_t>(m->lua_ai));number(b,static_cast<std::uint32_t>(m->only_lua_attack));number(b,static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(m->lua_state)));number(b,m->drop_random.seed());number(b,m->drop_random.words()[0]);}
        return b;
    }
};
int main(int argc,char** argv) {
    try {
        require(argc==2 || argc==3,"Expected zone mob release/death oracle");std::ifstream input(argv[1]);require(input.good(),"Cannot open release oracle");std::unique_ptr<State> state;std::string line;unsigned int scenarios=0,operations=0;
        while(std::getline(input,line)) {
            if(line.empty() || line[0]=='#') continue;std::istringstream row(line);char op=0;row>>op;
            if(op=='S') {unsigned int scenario=0;int extent=0;std::uint32_t counter=0;row>>scenario>>extent>>counter;state=std::make_unique<State>();state->initialize(scenario,extent,counter);++scenarios;continue;}
            auto& s=state->services;s.events.clear();unsigned int result=255;
            if(op=='A') {unsigned int fail=0;row>>fail>>s.time;s.fail=fail!=0;auto* m=state->pool->allocate();result=s.id(m);if(m) std::erase(s.free,m);}
            else if(op=='C') {unsigned int index=0,config=0;std::uint32_t id=0;row>>index>>config>>id;auto& m=*s.objects.at(index);m.id=id;m.status=static_cast<std::int32_t>(100+config);m.target_kind=static_cast<std::int32_t>(200+config);if(config&1) m.state_stack=std::vector<std::int32_t>{};else m.state_stack.reset();m.lua_ai=static_cast<std::int32_t>((config>>1)&1);m.only_lua_attack=static_cast<std::int32_t>(300+config);m.lua_state=config&4?reinterpret_cast<void*>(static_cast<std::uintptr_t>(0x70000000U+index*16)):nullptr;}
            else if(op=='H') {unsigned int index=0;std::int32_t status=0;std::uint32_t death=0;row>>index>>status>>death;auto& m=*s.objects.at(index);m.status=status;m.death_time=death;m.respawn=nullptr;}
            else if(op=='D') {unsigned int z=0;std::uint32_t now=0;row>>z>>now;s.entering_zone=z;state->zones.at(z).mobs->update_deaths(*state->pool,state->random,now,state->linked);}
            else if(op=='E') {unsigned int index=0,z=0;std::uint32_t x=0,y=0,iz=0;row>>index>>z>>x>>y>>iz;auto& m=*s.objects.at(index);m.position={std::bit_cast<float>(x),std::bit_cast<float>(y),std::bit_cast<float>(iz)};s.entering_zone=z;result=state->zones.at(z).mobs->enter(m)?1:0;}
            else {require(op=='R',"Unknown release operation");unsigned int z=0;row>>z;state->zones.at(z).release_mob_population(*state->pool);}
            std::string hex;row>>hex;require(static_cast<bool>(row),"Malformed release vector");Bytes actual;number(actual,result,1);number(actual,static_cast<std::uint32_t>(s.events.size()));actual.insert(actual.end(),s.events.begin(),s.events.end());const auto snapshot=state->snapshot();actual.insert(actual.end(),snapshot.begin(),snapshot.end());const auto expected=unhex(hex);if(actual!=expected) {const auto at=std::mismatch(actual.begin(),actual.end(),expected.begin(),expected.end());throw std::runtime_error("Original zone release differs at operation "+std::to_string(operations)+" byte "+std::to_string(at.first-actual.begin()));}++operations;
        }
        require(scenarios==128 && operations==(argc==3?2048U:3072U),"Zone release coverage changed");std::cout<<scenarios<<" original two-zone lifecycles, "<<operations<<" allocation/entry/release/reuse operations passed\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
