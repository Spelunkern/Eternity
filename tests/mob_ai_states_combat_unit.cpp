#include "eternity/game/mob_ai_states_combat.hpp"
#include "eternity/game/mob_ai_states.hpp"
#include <array>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace eternity::game;
void check(bool ok){if(!ok)throw std::runtime_error("Original combat state mismatch");}
using Fields=std::array<std::uint32_t,11>;
void assign(MobRuntime& m,const Fields& f){m.sleep=static_cast<std::int32_t>(f[0]);m.stun=static_cast<std::int32_t>(f[1]);m.attack_speed=static_cast<std::int32_t>(f[2]);m.only_lua_attack=static_cast<std::int32_t>(f[3]);m.lua_ai=static_cast<std::int32_t>(f[4]);m.next_attack=f[5];m.hp=f[6];m.lua_attacked_count=f[7];m.target_id=f[8];m.current_time=f[9];m.function_type=f[10];}
Fields fields(const MobRuntime& m){return {static_cast<std::uint32_t>(m.sleep),static_cast<std::uint32_t>(m.stun),static_cast<std::uint32_t>(m.attack_speed),static_cast<std::uint32_t>(m.only_lua_attack),static_cast<std::uint32_t>(m.lua_ai),m.next_attack,m.hp,m.lua_attacked_count,m.target_id,m.current_time,m.function_type};}
struct Services:MobAiCombatServices {
    MobRuntime& mob;MobAiCombatTarget target;unsigned mutation=0;std::uint32_t now=0;bool found=false,success=false;
    std::string trace;
    explicit Services(MobRuntime& subject):mob(subject){}
    void emit(char tag,std::initializer_list<std::uint32_t> values={}){
        if(!trace.empty())trace+=';';trace+=tag;trace+=':';bool first=true;
        for(auto value:values){if(!first)trace+=',';trace+=std::to_string(value);first=false;}
    }
    std::uint32_t tick() override {emit('T');if(mutation==1)mob.sleep=1;if(mutation==2){mob.sleep=0;mob.stun=0;mob.only_lua_attack=0;mob.next_attack=now;}return now;}
    MobAiCombatTarget* find_user(MobRuntime& m,std::uint32_t id) override {
        check(&m==&mob);emit('F',{id});if(mutation==3){m.only_lua_attack=1;m.lua_ai=1;m.next_attack=0xffffffff;}return found?&target:nullptr;
    }
    bool attack_ai(MobRuntime& m,std::uint32_t at,MobAiCombatTarget& user) override {
        check(&m==&mob && &user==&target);emit('A',{at});if(mutation==4){m.sleep=1;m.function_type=100;}return success;
    }
    void remove_vision_fix(MobRuntime& m) override {check(&m==&mob);emit('V');if(mutation==5)m.target_id=0x76543210;}
    void retarget(MobRuntime& m,std::uint32_t at) override {check(&m==&mob);emit('R',{at});}
    void lua_combat(MobRuntime& m,std::uint32_t at,std::uint32_t health,std::uint32_t count) override {
        check(&m==&mob);emit('L',{at,health,count});if(mutation==6){m.function_type=123;m.only_lua_attack=0;}
    }
};
struct StackServices:MobAiStateServices {
    MobAiCombatServices& combat;
    explicit StackServices(MobAiCombatServices& owner):combat(owner){}
    std::uint32_t tick() override {return combat.tick();}
    void lua_call(MobRuntime&,std::string_view,std::uint32_t,std::optional<std::uint32_t>) override {throw std::runtime_error("Unexpected event Lua");}
    void update(MobRuntime& mob,MobAiState state) override {check(state==MobAiState::combat);update_mob_ai_combat(mob,combat);}
    void error(MobRuntime&,MobAiError,std::int32_t,std::int32_t) override {throw std::runtime_error("Unexpected FSM error");}
};
int main(int argc,char**argv){
 try {
    check(argc==2);std::ifstream file(argv[1]);check(file.good());std::string line;unsigned cases=0;
    while(std::getline(file,line)){
        if(line.empty()||line[0]=='#')continue;std::istringstream row(line);unsigned id,mutation,found,success;std::uint32_t now,maximum,life;Fields input{},output{};std::string trace;
        row>>id>>mutation>>now>>maximum>>found>>life>>success;for(auto& value:input)row>>value;row>>trace;for(auto& value:output)row>>value;check(!row.fail());
        MobRuntime mob;MobRuntimeTemplate definition;definition.hp=maximum;mob.definition=&definition;assign(mob,input);
        Services services(mob);services.mutation=mutation;services.now=now;services.found=found!=0;services.success=success!=0;services.target={mob.target_id,static_cast<std::int32_t>(life)};
        // Exercise the real stack poll bridge for every original comparison.
        mob.state_stack=std::vector<std::int32_t>{0,1};StackServices stack(services);check(poll_mob_ai_stack(mob,stack));
        if(fields(mob)!=output || services.trace!=trace) throw std::runtime_error("Case "+std::to_string(id)+" actual="+services.trace+" expected="+trace);
        check(*mob.state_stack==std::vector<std::int32_t>({0,1}));++cases;
    }
    check(cases==1536);
    std::cout<<cases<<" original WhileCombat comparisons through native stack poll passed\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
