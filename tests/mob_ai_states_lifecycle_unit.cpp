#include "eternity/game/mob_ai_states_lifecycle.hpp"
#include "eternity/game/mob_pool.hpp"
#include "mob_damage_fixture.hpp"
#include <array>
#include <bit>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace eternity::game;
using Fields=std::array<std::uint32_t,18>;
void check(bool ok){if(!ok)throw std::runtime_error("Original Lua lifecycle mismatch");}
void* pointer(std::uint32_t value){return reinterpret_cast<void*>(static_cast<std::uintptr_t>(value));}
std::uint32_t handle(void* p){return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p));}
std::uint32_t stack_code(const MobRuntime& m){if(!m.state_stack||m.state_stack->empty())return 0;return *m.state_stack==std::vector<std::int32_t>{0}?1:2;}
Fields fields(const MobRuntime& m){return {handle(m.lua_state),m.state_stack?1U:0U,stack_code(m),m.retarget_time,m.lua_attacked_count,static_cast<std::uint32_t>(m.only_lua_attack),static_cast<std::uint32_t>(m.hold_position),m.lua_order_time,static_cast<std::uint32_t>(m.move_mode),std::bit_cast<std::uint32_t>(m.move_target.x),std::bit_cast<std::uint32_t>(m.move_target.y),std::bit_cast<std::uint32_t>(m.move_target.z),m.function_type,static_cast<std::uint32_t>(m.lua_ai),m.combat_timeout,m.lock_on_tick,m.current_time,static_cast<std::uint32_t>(m.status)};}
void assign(MobRuntime& m,const Fields& f){
 m.lua_state=pointer(f[0]);if(!f[1])m.state_stack.reset();else m.state_stack=f[2]==0?std::vector<std::int32_t>{}:f[2]==1?std::vector<std::int32_t>{0}:std::vector<std::int32_t>{4,1};
 m.retarget_time=f[3];m.lua_attacked_count=f[4];m.only_lua_attack=std::bit_cast<std::int32_t>(f[5]);m.hold_position=std::bit_cast<std::int32_t>(f[6]);m.lua_order_time=f[7];m.move_mode=std::bit_cast<std::int32_t>(f[8]);m.move_target={std::bit_cast<float>(f[9]),std::bit_cast<float>(f[10]),std::bit_cast<float>(f[11])};m.function_type=f[12];m.lua_ai=std::bit_cast<std::int32_t>(f[13]);m.combat_timeout=f[14];m.lock_on_tick=f[15];m.current_time=f[16];m.status=std::bit_cast<std::int32_t>(f[17]);
}
struct Services:MobAiLuaServices,MobAiStateServices {
 unsigned mutation=0,allocated=0;std::string trace;
 void event(MobRuntime& m,char tag,std::initializer_list<std::uint32_t> args={}){
    if(!trace.empty())trace+=';';trace+=tag;trace+=':';bool first=true;
    auto append=[&](auto value){if(!first)trace+=',';trace+=std::to_string(value);first=false;};
    for(auto value:args)append(value);for(auto value:fields(m))append(value);
 }
 void* create(MobRuntime& m) override {event(m,'N');if(mutation==1){m.lua_state=pointer(3);m.lua_ai=77;}return pointer(allocated);}
 void open_library(MobRuntime& m,void* state,MobAiLuaLibrary library) override {
    const auto tag=library==MobAiLuaLibrary::base?'B':library==MobAiLuaLibrary::math?'M':'O';event(m,tag,{handle(state)});
    if(mutation==2&&library==MobAiLuaLibrary::base){m.lua_state=pointer(3);m.lua_ai=88;}
    if(mutation==3&&library==MobAiLuaLibrary::math){m.lua_state=nullptr;m.only_lua_attack=99;}
 }
 void register_mob(MobRuntime& m,void* state) override {event(m,'R',{handle(state)});if(mutation==4)disable_mob_ai_lua(m);if(mutation==5){m.lua_ai=5;m.function_type=123;}}
 void close(MobRuntime& m,void* state) override {event(m,'C',{handle(state)});if(mutation==6){m.lua_state=pointer(3);m.lua_ai=6;m.only_lua_attack=7;}}
 void call_init(MobRuntime& m,void* state) override {event(m,'I',{handle(state)});if(mutation==7)disable_mob_ai_lua(m);}
 std::uint32_t tick() override {throw std::runtime_error("Unexpected tick");}
 void update(MobRuntime&,MobAiState) override {throw std::runtime_error("Unexpected state update");}
 void error(MobRuntime&,MobAiError,std::int32_t,std::int32_t) override {throw std::runtime_error("Unexpected FSM error");}
 void lua_call(MobRuntime&,std::string_view,std::uint32_t,std::optional<std::uint32_t>) override {throw std::runtime_error("Unexpected event Lua");}
};
struct PoolServices:MobPoolServices {
 Services& vm;MobDamageFixture& heap;MobRuntime* subject=nullptr;std::string order;
 PoolServices(Services& owner,MobDamageFixture& storage):vm(owner),heap(storage){}
 std::unique_ptr<MobRuntime> construct() override {auto result=std::make_unique<MobRuntime>();subject=result.get();return result;}
 std::uint32_t unix_time() override {return 1;}
 MobDamagePool& damage_pool() override {return heap.pool;}
 void release_compiled_ai(MobRuntime&) override {order+='f';}
 void release_stack_ai(MobRuntime& mob) override {order+='s';mob.state_stack.reset();}
 void close_lua(void* state) override {order+='c';vm.close(*subject,state);}
};
int main(int argc,char**argv){try{
 check(argc==2);std::ifstream file(argv[1]);check(file.good());std::string line;unsigned count=0;
 while(std::getline(file,line)){
    if(line.empty()||line[0]=='#')continue;std::istringstream row(line);unsigned id,operation,mutation,allocated,result;Fields input{},expected{};std::string trace;
    row>>id>>operation>>mutation>>allocated;for(auto& value:input)row>>value;row>>trace;for(auto& value:expected)row>>value;row>>result;check(!row.fail());
    MobRuntime mob;assign(mob,input);Services services;services.mutation=mutation;services.allocated=allocated;unsigned actual=0;
    switch(operation){case 0:actual=initialize_mob_ai_vm(mob,services)?1U:0U;break;case 1:release_mob_ai_vm(mob,services);break;case 2:initialize_mob_ai_lua_fsm(mob,services,services);break;case 3:release_mob_ai_lua_fsm(mob,services);break;case 4:disable_mob_ai_lua(mob);break;case 5:call_mob_ai_lua_init(mob,services);break;default:check(false);}
    if(trace=="-")trace.clear();if(actual!=result || fields(mob)!=expected || services.trace!=trace)throw std::runtime_error("Case "+std::to_string(id)+" actual="+services.trace+" expected="+trace);++count;
 }
 check(count==768);
 // Compose the actual stack lifecycle with a script error and death transition.
 MobRuntime mob;Services services;services.allocated=1;initialize_mob_ai_lua_fsm(mob,services,services);check(mob.state_stack && *mob.state_stack==std::vector<std::int32_t>{0} && mob.lua_ai==1);
 disable_mob_ai_lua(mob);check(mob.lua_state==pointer(1));dispatch_mob_ai_event(mob,3,0,services);check(mob.state_stack->back()==2 && poll_mob_ai_stack(mob,services));
 release_mob_ai_lua_fsm(mob,services);check(!mob.state_stack && !mob.lua_state);
 // The existing pool owns its own release sequence. Its provider adapter must
 // release only stack storage, rather than recursively invoke the whole VM/FSM
 // wrapper. Compare its actual close callback and retained fields with the
 // standalone recovered release, including callback writes after flag clearing.
 MobDamageFixture damage;Services pooled_vm;pooled_vm.allocated=1;
 PoolServices ports(pooled_vm,damage);std::uint32_t counter=0;MobPool pool(counter,ports);
 auto* pooled=pool.allocate();check(pooled!=nullptr);initialize_mob_ai_lua_fsm(*pooled,pooled_vm,pooled_vm);pooled->only_lua_attack=9;
 MobRuntime expected;assign(expected,fields(*pooled));Services expected_vm;expected_vm.mutation=6;
 pooled_vm.trace.clear();pooled_vm.mutation=6;release_mob_ai_lua_fsm(expected,expected_vm);pool.release(*pooled);
 check(fields(*pooled)==fields(expected) && pooled_vm.trace==expected_vm.trace && ports.order=="fsc" && pool.available()==1);
 std::cout<<count<<" original Lua/FSM cases; real stack/death/release and MobPool release composition passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
