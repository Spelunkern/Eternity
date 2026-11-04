#include "eternity/game/mob_ai_states.hpp"
#include "mob_damage_fixture.hpp"
#include "mob_death_reward_fixture.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace eternity::game;
void check(bool ok) {if(!ok) throw std::runtime_error("Original mob AI stack mismatch");}
std::vector<std::int32_t> numbers(const std::string& text) {
    std::vector<std::int32_t> result; if(text=="-") return result;
    std::istringstream in(text); std::string word; while(std::getline(in,word,',')) result.push_back(static_cast<std::int32_t>(std::stoul(word,nullptr,0))); return result;
}
struct Services:MobAiStateServices {
    std::uint32_t now=0,call_tick=0,argument=0; std::string name="-";
    std::vector<std::int32_t> observed,mutation,updates,errors;
    std::uint32_t tick() override {return now;}
    void lua_call(MobRuntime& mob,std::string_view function,std::uint32_t at,std::optional<std::uint32_t> value) override {
        name=function;call_tick=at;argument=value.value_or(0); observed=*mob.state_stack;
        if(!mutation.empty()) mob.state_stack=mutation;
    }
    void update(MobRuntime&,MobAiState state) override {updates.push_back(static_cast<std::int32_t>(state));}
    void error(MobRuntime&,MobAiError error,std::int32_t state,std::int32_t) override {
        switch(error) {
        case MobAiError::empty_dispatch:errors.push_back(0x53895c);break;
        case MobAiError::unhandled_event:errors.push_back(0x538924);break;
        case MobAiError::overflow:errors.push_back(0x5388e8);break;
        case MobAiError::empty_pop:errors.push_back(0x5388b0);break;
        case MobAiError::empty_poll:errors.push_back(0x538a20);break;
        case MobAiError::invalid_initial:errors.push_back(state==-1?0x538880:0x538858);break;
        }
    }
};
int main(int argc,char**argv) {
 try {
    check(argc==2);std::ifstream file(argv[1]);check(file.good());std::string line;unsigned cases=0,states=0,transitions=0;
    const std::array<std::uint32_t,5> handlers{0x4b52c0,0x4b55c0,0x4b5780,0x4b5c40,0x4b5790};
    const auto callback=[](MobEvent action) -> std::uint32_t {
        switch(action) {case MobEvent::attackable:return 0x4b6170;case MobEvent::attacked:return 0x4b6120;case MobEvent::move:return 0x4b60d0;case MobEvent::move_end:return 0x4b60e0;case MobEvent::normal_reset:return 0x4b61c0;case MobEvent::death:return 0x4b6220;case MobEvent::return_home:return 0x4b6270;default:return 0;}
    };
    while(std::getline(file,line)) {
        if(line.empty() || line[0]=='#')continue;std::istringstream row(line);char kind;row>>kind;
        if(kind=='S') {unsigned id,handler,enter,leave;row>>id>>handler>>enter>>leave;check(id==states++ && handler==handlers.at(id) && !enter && !leave);}
        else if(kind=='R') {
            int source,event,to,op;unsigned function;row>>source>>event>>to>>op>>function;
            const auto& entry=mob_ai_transitions()[transitions++];
            check(entry.from==source && entry.event==event && entry.to==to && static_cast<int>(entry.operation)==op && callback(entry.action)==function);
        } else if(kind=='I') {
            int initial,released;std::string final,errors;row>>initial>>final>>released>>errors;
            MobRuntime mob;mob.state_stack=std::vector<std::int32_t>{4,1};Services ports;
            initialize_mob_ai_stack(mob,initial,ports);
            check(mob.state_stack.has_value()==!released && ports.errors==numbers(errors));
            if(mob.state_stack)check(*mob.state_stack==numbers(final));
        } else if(kind=='P') {
            std::string initial,errors;unsigned result;row>>initial>>result>>errors;
            MobRuntime mob;mob.state_stack=numbers(initial);Services ports;
            check(poll_mob_ai_stack(mob,ports)==(result!=0) && ports.errors==numbers(errors));
        } else {
            unsigned id,lua;int event;std::uint32_t now,ct,ca;std::string initial,mutation,name,observed,final,errors,updates;
            row>>id>>event>>lua>>initial>>mutation>>now>>name>>ct>>ca>>observed>>final>>errors>>updates;
            std::array<std::uint32_t,7> expected{};for(auto& v:expected)row>>v;check(!row.fail());
            MobRuntime mob;mob.state_stack=numbers(initial);mob.status=77;mob.lua_ai=static_cast<int>(lua);mob.only_lua_attack=9;mob.combat_timeout=123;mob.current_attack=7;mob.move_mode=8;mob.function_type=99;mob.lua_attacked_count=0x80000000U+id;
            Services ports;ports.now=now;ports.mutation=numbers(mutation);
            dispatch_mob_ai_event(mob,event,0x12340000U+id,ports);check(poll_mob_ai_stack(mob,ports));
            const std::array<std::uint32_t,7> actual{static_cast<std::uint32_t>(mob.lua_ai),static_cast<std::uint32_t>(mob.only_lua_attack),mob.combat_timeout,static_cast<std::uint32_t>(mob.current_attack),static_cast<std::uint32_t>(mob.move_mode),mob.function_type,mob.lua_attacked_count};
            if(!(actual==expected && *mob.state_stack==numbers(final) && ports.name==name && ports.call_tick==ct && ports.argument==ca && ports.observed==numbers(observed) && ports.errors==numbers(errors) && ports.updates==numbers(updates) && mob.status==77)) throw std::runtime_error("Case "+std::to_string(id)+" mismatch");
            ++cases;
        }
    }
    check(states==5 && transitions==23 && cases==480);
    // Real death reward coordinator remains a separate combat caller, after the
    // event has put the mob into death. Polling the corpse never repeats rewards.
    MobDamageFixture heap;MobRuntime mob;MobRuntimeTemplate definition;definition.level=1;definition.experience=100;
    mob.definition=&definition;mob.damage=std::make_unique<MobDamageLedger>(heap.pool);mob.damage->add(1,42,100,0,0);
    MobDeathRewardFixture world;world.user={42,1,0};unsigned awards=0;
    world.reward=[&](auto&,auto,auto amount){check(amount>0 && mob.state_stack->back()==2);++awards;};
    Services services;initialize_mob_ai_stack(mob,0,services);dispatch_mob_ai_event(mob,3,123,services);
    check(mob.state_stack->back()==2 && world.order.empty());
    mob_death(mob,1,42,MobDeathExperience{},world);check(awards==1 && mob.damage->records().empty());
    for(unsigned i=0;i<5;++i) check(poll_mob_ai_stack(mob,services));
    check(awards==1 && services.updates.empty());
    std::cout<<cases<<" original dispatch/poll cases; 5 states, 23 transitions; real death reward composition passed\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
