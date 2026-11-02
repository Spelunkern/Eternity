#include "eternity/game/zone_mobs.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace eternity::game;
using Bytes=eternity::net::Packet;
void require(bool v,const char* message) {if(!v) throw std::runtime_error(message);}
void number(Bytes& b,std::uint32_t v,unsigned int n=4) {for(unsigned int i=0;i<n;++i) b.push_back(static_cast<std::uint8_t>(v>>(8*i)));}
Bytes unhex(const std::string& text) {Bytes b;for(std::size_t i=0;i<text.size();i+=2) b.push_back(static_cast<std::uint8_t>(std::stoul(text.substr(i,2),nullptr,16)));return b;}
struct Services final:ZoneMobServices,MobViewer {
    Bytes events;std::array<MobRuntime,4> mobs;ZoneMobs* zone=nullptr;CellCoordinates cell;bool reenter=false;
    unsigned int id(const MobRuntime* mob) const {if(!mob) return 255;for(unsigned int i=0;i<mobs.size();++i) if(mob==&mobs[i]) return i;throw std::runtime_error("Unknown mob pointer");}
    void event(std::uint8_t tag,const Bytes& p) {events.push_back(tag);number(events,static_cast<std::uint32_t>(p.size()));events.insert(events.end(),p.begin(),p.end());}
    std::uint32_t tick_ms() override {throw std::runtime_error("Unexpected clock");}
    std::unique_ptr<MobBroadcast> allocate_broadcast(const Bytes&,std::optional<std::uint8_t>) override {throw std::runtime_error("Unexpected movement buffer");}
    void send_cell_view(CellCoordinates c,const Bytes& p) override {
        // Entry callbacks must already see both indexes and copied respawn position.
        const auto list=zone->in_cell(c);require(!list.empty(),"Entry sent before cell insertion");auto* m=list.back();require(m->zone==zone && m->cell.has_value(),"Entry sent before binding");require(m->respawn_position.x==m->position.x,"Entry sent before respawn position assignment");event(1,p);
    }
    void lua_init(MobRuntime& m) override {require(!events.empty(),"Lua called before entry broadcast");event(2,Bytes{static_cast<std::uint8_t>(id(&m))});}
    void send_self(const Bytes& p) override {event(3,p);if(reenter) {reenter=false;require(zone->find_cell(cell,0x76543210)==nullptr,"Reentrant missing ID unexpectedly found");}}
};
struct State {
    Services services;std::unique_ptr<ZoneMobs> zone;
    void initialize(unsigned int scenario) {
        zone.reset();for(auto& m:services.mobs) m={};zone=std::make_unique<ZoneMobs>(MapGeometry(81),services);services.zone=zone.get();
        const std::array<std::uint32_t,4> ids{0,0x80000000,0xffffffff,1234};
        for(unsigned int i=0;i<4;++i) {
            auto& m=services.mobs[i];m.id=ids[i<2?scenario%4:(scenario+i)%4];auto x=scenario%7==0?-0.25F:1.25F;if(i==2 && scenario%2) x=81.25F;if(i==3) x=121.25F;
            m.position={x,-static_cast<float>(i)-0.5F,3.25F};m.type=static_cast<std::uint16_t>(scenario*1000+i);m.lua_ai=(scenario+i)%3==0?1:0;m.status=std::array{0,2,100}[(scenario+i)%3];
            const auto mode=(scenario+i)%5;if(mode) {m.state_stack=std::vector<std::int32_t>{};if(mode>1) m.state_stack->assign(mode-1,(scenario+i)%2?2:100);}
        }
    }
    Bytes snapshot() {
        Bytes b;number(b,static_cast<std::uint32_t>(zone->size()));const auto list=zone->all();number(b,static_cast<std::uint32_t>(list.size()),1);for(auto* m:list) number(b,services.id(m),1);
        for(auto& m:services.mobs) {number(b,m.zone?1:0,1);number(b,m.cell?static_cast<std::uint32_t>(m.cell->x):0);number(b,m.cell?static_cast<std::uint32_t>(m.cell->z):0);for(auto f:{m.respawn_position.x,m.respawn_position.y,m.respawn_position.z}) number(b,std::bit_cast<std::uint32_t>(f));number(b,services.id(zone->find(m.id)),1);}
        for(int z=0;z<3;++z) for(int x=0;x<3;++x) {const auto cell=zone->in_cell({x,z});number(b,static_cast<std::uint32_t>(cell.size()),1);for(auto* m:cell) number(b,services.id(m),1);for(auto& m:services.mobs) number(b,services.id(zone->find_cell({x,z},m.id)),1);}
        return b;
    }
};
int main(int argc,char** argv) {
    try {
        require(argc==2,"Expected zone mobs oracle");std::ifstream input(argv[1]);require(input.good(),"Cannot open zone mobs oracle");State state;std::string line;unsigned int scenarios=0,operations=0;
        while(std::getline(input,line)) {
            if(line.empty() || line[0]=='#') continue;std::istringstream row(line);char op=0;row>>op;
            if(op=='S') {unsigned int scenario=0;row>>scenario;state.initialize(scenario);++scenarios;continue;}
            state.services.events.clear();unsigned int result=0;
            if(op=='E' || op=='L') {unsigned int index=0;row>>index;auto& m=state.services.mobs.at(index);result=op=='E'?state.zone->enter(m):state.zone->leave(m);}
            else {require(op=='V' || op=='W',"Unknown mob registry operation");unsigned int reenter=0;row>>state.services.cell.x>>state.services.cell.z>>reenter;state.services.reenter=reenter!=0;if(op=='V') state.zone->send_enter(state.services.cell,state.services);else state.zone->send_leave(state.services.cell,state.services);}
            std::string expected;row>>expected;require(static_cast<bool>(row),"Malformed registry vector");std::string extra;require(!(row>>extra),"Trailing registry data");Bytes actual;number(actual,result,1);number(actual,static_cast<std::uint32_t>(state.services.events.size()));actual.insert(actual.end(),state.services.events.begin(),state.services.events.end());const auto snapshot=state.snapshot();actual.insert(actual.end(),snapshot.begin(),snapshot.end());const auto original=unhex(expected);
            if(actual!=original) {const auto at=std::mismatch(actual.begin(),actual.end(),original.begin(),original.end());throw std::runtime_error("Original zone mobs differ at operation "+std::to_string(operations)+" byte "+std::to_string(at.first-actual.begin()));}++operations;
        }
        require(scenarios==128 && operations==2048,"Mob registry coverage changed");std::cout<<scenarios<<" original mob registries, "<<operations<<" membership/snapshot operations passed\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
