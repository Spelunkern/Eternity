#include "eternity/game/mob_ai_states_configured.hpp"
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
using namespace eternity::game;
void check(bool ok){if(!ok)throw std::runtime_error("Original configured FSM mismatch");}
constexpr std::array<std::uint32_t,5> codes{10,20,30,0xffffffff,0x80000000};
MobAiConfiguredAction& action(MobAiConfiguredState& state,unsigned phase){return phase==0?state.enter:phase==1?state.update:state.exit;}
MobAiConfiguredProgram& program(MobAiConfiguredState& state,unsigned phase){return phase==0?state.enter_program:phase==1?state.update_program:state.exit_program;}
struct Services:MobAiConfiguredServices{
 unsigned mutation=0;std::string trace;
 void event(char tag,std::uint32_t identity,MobRuntime& mob,MobAiConfiguredFsm& fsm){if(!trace.empty())trace+=';';trace+=tag;trace+=':';trace+=std::to_string(identity)+','+std::to_string(fsm.current)+','+std::to_string(static_cast<std::uint32_t>(mob.current_attack));}
 void invoke(MobRuntime& mob,MobAiConfiguredFsm& fsm,std::uint32_t code,unsigned phase){
    event("EUX"[phase],code,mob,fsm);mob.current_attack=std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(mob.current_attack)+phase+1U);
    if(mutation==3&&phase==2){fsm.current=0xffffffff;fsm.states[10].transitions[0].target=10;}
    if(mutation==4&&phase==0)fsm.current=0x80000000;
    if(mutation==5&&phase==1)fsm.current=77;
 }
 std::int32_t travel_condition(MobRuntime& mob,MobAiConfiguredFsm& fsm,MobAiConfiguredCondition& condition) override {
    const auto id=condition.words[0],value=condition.words[1];event('Q',id,mob,fsm);
    if(id<1000){auto& transition=fsm.states[10].transitions[(id-1)/10];
        if(mutation==1&&id==1){fsm.current=20;transition.target=30;}
        if(mutation==2&&id==1){std::uint32_t sum=0;for(const auto& item:transition.conditions)sum+=item.words[1];transition.expected_sum=std::bit_cast<std::int32_t>(sum);}
        if(mutation==8&&id==1&&transition.conditions.size()>=2)transition.conditions[1].words[1]=1;
    }else if(id%10==0){const auto index=(id-1000)/100,phase=(id%100)/10;auto& state=fsm.states[codes.at(index)];auto& body=program(state,phase);
        if(mutation==6){body.count=3;body.enabled=false;}
        if(mutation==7){body.count=0;action(state,phase)={};}
    }
    return std::bit_cast<std::int32_t>(value);
 }
};
std::string snapshot(const MobAiConfiguredFsm& fsm){
 std::string result;
 for(const auto& [code,state]:fsm.states){
    if(!result.empty())result+=';';unsigned mask=(state.enter?1U:0U)|(state.update?2U:0U)|(state.exit?4U:0U);unsigned enabled=(state.enter_program.enabled?1U:0U)|(state.update_program.enabled?2U:0U)|(state.exit_program.enabled?4U:0U);
    result+=std::to_string(code)+','+std::to_string(mask)+','+std::to_string(enabled);
    auto add=[&](auto value){result+=',';result+=std::to_string(static_cast<std::uint32_t>(value));};
    add(state.enter_program.count);add(state.update_program.count);add(state.exit_program.count);
    for(const auto& transition:state.transitions){add(transition.target);add(transition.expected_sum);for(const auto& condition:transition.conditions)add(condition.words[1]);}
 }
 return result;
}
int main(int argc,char**argv){try{
 check(argc==2);std::ifstream file(argv[1]);check(file.good());MobRuntime mob;std::unique_ptr<MobAiConfiguredFsm> fsm;Services services;unsigned id=0,operation=0,count=0;std::string line;
 while(std::getline(file,line)){
    if(line.empty()||line[0]=='#')continue;std::istringstream row(line);char tag;row>>tag;
    if(tag=='C'){unsigned mutation;std::uint32_t current,start,counter;row>>id>>operation>>mutation>>current>>start>>counter;fsm=std::make_unique<MobAiConfiguredFsm>(current,start);mob.current_attack=std::bit_cast<std::int32_t>(counter);services.mutation=mutation;services.trace.clear();}
    else if(tag=='S'){
        std::uint32_t code,mask,enabled,base;std::array<std::uint32_t,3> counts{};row>>code>>mask>>enabled>>counts[0]>>counts[1]>>counts[2]>>base;auto& state=fsm->states[code];
        for(unsigned phase=0;phase<3;++phase){if((mask>>phase)&1U)action(state,phase)=[&services,code,phase](MobRuntime& subject,MobAiConfiguredFsm& owner){services.invoke(subject,owner,code,phase);};auto& body=program(state,phase);body.enabled=((enabled>>phase)&1U)!=0;body.count=std::bit_cast<std::int32_t>(counts[phase]);for(unsigned j=0;j<3;++j){MobAiConfiguredCondition expression;expression.words[0]=base+phase*10+j;expression.words[1]=j%2;body.expressions.push_back(expression);}}
    }else if(tag=='T'){
        std::uint32_t expected,target,size;row>>expected>>target>>size;MobAiConfiguredTransition transition;transition.expected_sum=std::bit_cast<std::int32_t>(expected);transition.target=target;for(unsigned j=0;j<size;++j){MobAiConfiguredCondition condition;for(auto& word:condition.words)row>>word;transition.conditions.push_back(condition);}fsm->states[10].transitions.push_back(std::move(transition));
    }else if(tag=='E'){
        std::string trace,state;std::uint32_t current,counter;row>>trace>>current>>counter>>state;
        if(operation==0)check(start_mob_ai_configured(mob,*fsm,services));else if(operation==1)execute_mob_ai_configured(mob,*fsm,services);else if(operation==2)evaluate_mob_ai_configured(mob,*fsm,services);else{execute_mob_ai_configured(mob,*fsm,services);evaluate_mob_ai_configured(mob,*fsm,services);}
        if(trace=="-")trace.clear();if(fsm->current!=current||static_cast<std::uint32_t>(mob.current_attack)!=counter||services.trace!=trace||snapshot(*fsm)!=state)throw std::runtime_error("Case "+std::to_string(id)+" actual="+services.trace+" expected="+trace+" current="+std::to_string(fsm->current)+"/"+std::to_string(current));++count;
    }else check(false);
    check(!row.fail());
 }
 check(count==768);std::cout<<count<<" original configured FSM cases with native retained state ownership passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
