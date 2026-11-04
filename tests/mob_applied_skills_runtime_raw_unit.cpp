#include "mob_damage_fixture.hpp"
#include "eternity/game/mob_pool.hpp"
#include <algorithm>
#include <bit>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace eternity::game;using Bytes=std::vector<std::uint8_t>;
void require(bool v,const char* s) {if(!v) throw std::runtime_error(s);}
void number(Bytes& b,std::uint32_t v,unsigned int n=4) {for(unsigned int i=0;i<n;++i) b.push_back(static_cast<std::uint8_t>(v>>(8*i)));}
Bytes unhex(const std::string& text) {Bytes b;for(std::size_t i=0;i<text.size();i+=2) b.push_back(static_cast<std::uint8_t>(std::stoul(text.substr(i,2),nullptr,16)));return b;}
struct Services final:MobPoolServices {
    MobDamageFixture damage_fixture;MobDamagePool& damage_pool() override {return damage_fixture.pool;}

    std::vector<MobRuntime*> objects;Bytes events;bool fail=false,constructing=false;std::uint32_t time=0;
    unsigned int id(const MobRuntime* m) const {if(!m) return 255;const auto at=std::find(objects.begin(),objects.end(),m);require(at!=objects.end(),"Unknown pool object");return static_cast<unsigned int>(at-objects.begin());}
    std::uint32_t unix_time() override {if(!constructing) events.push_back('I');events.push_back('T');return time;}
    std::unique_ptr<MobRuntime> construct() override {
        events.push_back('N');if(fail) return {};auto m=std::make_unique<MobRuntime>();constructing=true;m->drop_random=ServerRandom(unix_time());constructing=false;
        // Unassigned source constructor fields carry a known allocator pattern.
        const auto value=std::bit_cast<float>(0xa5a5a5a5U);m->position={value,value,value};m->only_lua_attack=std::bit_cast<std::int32_t>(0xa5a5a5a5U);objects.push_back(m.get());return m;
    }
    void release_compiled_ai(MobRuntime&) override {events.push_back('C');}
    void release_stack_ai(MobRuntime&) override {events.push_back('S');}
    void close_lua(void* state) override {events.push_back('L');number(events,static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(state)));}
};
struct State {
    std::uint32_t counter=0;Services services;std::unique_ptr<MobPool> pool;std::vector<MobRuntime*> free;std::vector<MobRuntime*> prepared;
    void initialize(std::uint32_t value) {pool.reset();services.objects.clear();free.clear();counter=value;pool=std::make_unique<MobPool>(counter,services);}
    Bytes snapshot() {
        Bytes b;number(b,counter);number(b,pool->construction_attempts());number(b,static_cast<std::uint32_t>(pool->available()));number(b,static_cast<std::uint32_t>(pool->size()));number(b,static_cast<std::uint32_t>(free.size()),1);for(auto at=free.rbegin();at!=free.rend();++at) number(b,services.id(*at),1);
        for(auto* m:services.objects) {number(b,m->id);number(b,static_cast<std::uint32_t>(m->status));number(b,static_cast<std::uint32_t>(m->target_kind));for(auto f:{m->position.x,m->position.y,m->position.z}) number(b,std::bit_cast<std::uint32_t>(f));number(b,m->state_stack?1:0);number(b,m->state_stack?static_cast<std::uint32_t>(m->state_stack->size()):0);number(b,static_cast<std::uint32_t>(m->lua_ai));number(b,static_cast<std::uint32_t>(m->only_lua_attack));number(b,static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(m->lua_state)));number(b,m->drop_random.seed());const bool seeded=std::find(prepared.begin(),prepared.end(),m)!=prepared.end();for(std::size_t i=0;i<4;++i)number(b,seeded?m->drop_random.words()[i]:0);number(b,seeded?m->drop_random.words().back():0);number(b,m->drop_random.index());
            require(static_cast<bool>(m->damage),"Pool mob lacks its damage owner");number(b,m->damage->fair_combat?1:0,1);number(b,static_cast<std::uint32_t>(m->damage->records().size()));
            for(auto* record:m->damage->records()) {number(b,record->type,1);number(b,record->id);number(b,static_cast<std::uint32_t>(record->damage));number(b,static_cast<std::uint32_t>(record->aggro));number(b,record->party);}
        }
        return b;
    }
};
int main(int argc,char** argv) {
    try {
        require(argc==2,"Expected mob pool oracle");std::ifstream input(argv[1]);require(input.good(),"Cannot open mob pool oracle");std::unique_ptr<State> owned;std::string line;unsigned int scenarios=0,operations=0;
        while(std::getline(input,line)) {
            if(line.empty() || line[0]=='#') continue;std::istringstream row(line);char op=0;row>>op;
            if(op=='B') {unsigned int scenario=0;std::uint32_t counter=0;row>>scenario>>counter;owned=std::make_unique<State>();owned->initialize(counter);++scenarios;continue;}
            auto& state=*owned;state.services.events.clear();unsigned int result=255;
            if(op=='A' || op=='R') {unsigned int fail=0;row>>fail>>state.services.time;state.services.fail=fail!=0;auto* m=op=='A'?state.pool->allocate():state.pool->allocate_raw();result=state.services.id(m);if(m){std::erase(state.free,m);if(op=='A')state.prepared.push_back(m);}}
            else {
                unsigned int index=0;row>>index;auto& m=*state.services.objects.at(index);
                if(op=='P'){row>>state.services.time;state.pool->prepare_allocated(m);state.prepared.push_back(&m);}
                else if(op=='G') m.damage->add(1,111,INT32_MAX,100,7);
                else if(op=='H') m.damage->reset();
                else if(op=='C') {unsigned int config=0;row>>config;m.status=static_cast<std::int32_t>(0x12340000U+config);m.target_kind=std::bit_cast<std::int32_t>(0x87650000U+config);m.position={static_cast<float>(config)+0.25F,-3.5F,static_cast<float>(config)+1.75F};if(config&1) m.state_stack=std::vector<std::int32_t>{};else m.state_stack.reset();m.lua_ai=static_cast<std::int32_t>((config>>1)&1);m.only_lua_attack=std::bit_cast<std::int32_t>(0x98760000U+config);m.lua_state=config&4?reinterpret_cast<void*>(static_cast<std::uintptr_t>(0x70000000U+index*16)):nullptr;}
                else {require(op=='F',"Unknown pool operation");state.pool->release(m);state.free.push_back(&m);}
            }
            std::string expected;row>>expected;require(static_cast<bool>(row),"Malformed pool vector");Bytes actual;number(actual,result,1);number(actual,static_cast<std::uint32_t>(state.services.events.size()));actual.insert(actual.end(),state.services.events.begin(),state.services.events.end());const auto snapshot=state.snapshot();actual.insert(actual.end(),snapshot.begin(),snapshot.end());const auto original=unhex(expected);
            if(actual!=original) {const auto at=std::mismatch(actual.begin(),actual.end(),original.begin(),original.end());throw std::runtime_error("Original mob pool differs at operation "+std::to_string(operations)+" byte "+std::to_string(at.first-actual.begin()));}++operations;
        }
        require(scenarios==128 && operations==2816,"Mob pool coverage changed");std::cout<<scenarios<<" original pools, "<<operations<<" raw/preparation/manager/reuse operations passed\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
