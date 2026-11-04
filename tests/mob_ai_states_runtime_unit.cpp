#include "eternity/game/mob_ai_states_runtime.hpp"
#include <array>
#include <bit>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace eternity::game;
using Fields=std::array<std::uint32_t,11>;
unsigned current_case=0;
void check(bool ok){if(!ok)throw std::runtime_error("Original factory startup mismatch case "+std::to_string(current_case));}
std::string decode(const std::string& text){if(text=="-")return {};std::string result;for(std::size_t i=0;i<text.size();i+=2)result+=static_cast<char>(std::stoul(text.substr(i,2),nullptr,16));return result;}
struct Services:MobAiRandomServices,MobAiBattleServices,MobAiDebugServices {
 MobRuntime& mob;MobAiConfiguredFsm& fsm;std::uint32_t code;unsigned mutation=0;std::string trace;
 Services(MobRuntime& m,MobAiConfiguredFsm& owner,std::uint32_t key):mob(m),fsm(owner),code(key){}
 Fields fields(){return {fsm.current,fsm.start,static_cast<std::uint32_t>(mob.status),static_cast<std::uint32_t>(mob.target_kind),mob.target_id,mob.attacked?1U:0U,mob.in_battle?1U:0U,mob.vision_fixed_user,static_cast<std::uint32_t>(fsm.variables.at(7).integer),static_cast<std::uint32_t>(fsm.compiled_functions.size()),static_cast<std::uint32_t>(fsm.states.at(code).enter_program.count)};}
 void emit(char tag,std::string extra={}){if(!trace.empty())trace+=';';trace+=tag;trace+=':';if(!extra.empty()){trace+=extra;trace+=',';}bool first=true;for(auto value:fields()){if(!first)trace+=',';first=false;trace+=std::to_string(value);}}
 void write(std::string_view text) override {check(text=="PAI Debug on\n");emit('D',"504149204465627567206f6e0a");if(mutation==3){fsm.current=99;fsm.start=88;mob.target_id=77;}if(mutation==5)mob.vision_fixed_user=9;}
 void enter(){emit('E');if(mutation==3){fsm.current=99;fsm.start=88;mob.target_id=77;}if(mutation==5)mob.status=18;}
 std::int32_t travel_condition(MobRuntime&,MobAiConfiguredFsm&,MobAiConfiguredCondition& expression) override {emit('Q',std::to_string(expression.words[0]));if(mutation==1)fsm.start=77;if(mutation==2)fsm.states.at(code).enter={};if(mutation==4){fsm.states.at(code).enter_program.count=0;mob.target_id=55;}return 1;}
 std::int32_t random() override {throw std::runtime_error("Unexpected random callback");}
 void move(MobRuntime&,UserPosition,std::int32_t) override {throw std::runtime_error("Unexpected movement");}
 MobAiBattleTarget* find_user(MobRuntime&,std::uint32_t) override {throw std::runtime_error("Unexpected lookup");}
 void remove_vision_fix(MobRuntime&) override {throw std::runtime_error("Unexpected vision cleanup");}
 MobAiBattleTarget* find_target(MobRuntime&) override {throw std::runtime_error("Unexpected retarget");}
 void attack_ai(MobRuntime&,std::uint32_t,MobAiBattleTarget&) override {throw std::runtime_error("Unexpected attack");}
};
int main(int argc,char**argv){try{
 check(argc==2);std::ifstream file(argv[1]);check(file.good());std::string line;unsigned cases=0;
 while(std::getline(file,line)){
  if(line.empty()||line[0]=='#')continue;std::istringstream row(line);std::string start,enter,expected,locks;unsigned bind,enabled,callback,mutation,rounds;Fields input{},output{};row>>current_case>>start>>enter>>bind>>enabled>>callback>>mutation>>rounds;for(auto& value:input)row>>value;row>>expected>>locks;for(auto& value:output)row>>value;check(!row.fail());start=decode(start);enter=decode(enter);
  MobRuntime mob;MobAiConfiguredFsm fsm(input[0],input[1]);const auto code=mob_ai_name_code(start);Services services(mob,fsm,code);services.mutation=mutation;mob.status=std::bit_cast<std::int32_t>(input[2]);mob.target_kind=std::bit_cast<std::int32_t>(input[3]);mob.target_id=input[4];mob.attacked=input[5]!=0;mob.in_battle=input[6]!=0;mob.vision_fixed_user=input[7];MobAiParameter variable(1);variable.integer=std::bit_cast<std::int32_t>(input[8]);fsm.variables.emplace(7,variable);
  auto& state=fsm.states[code];state.enter_program.enabled=enabled!=0;state.enter_program.count=std::bit_cast<std::int32_t>(input[10]);for(unsigned i=0;i<3;++i){MobAiConfiguredCondition expression;expression.words[0]=i+1;state.enter_program.expressions.push_back(expression);}if(callback)state.enter=[&](MobRuntime&,MobAiConfiguredFsm&){services.enter();};
  MapDefinition map;MobAiRandomDefinition random_definition;MobAiBattleDefinition battle_definition;MobAiVariableCatalog catalog;MobAiCompiledRuntime runtime(map,random_definition,battle_definition,catalog,services,services,services);std::vector<MobAiCompiledStateNames> bindings;if(bind)bindings.push_back({start,enter,"",""});
  for(unsigned i=0;i<rounds;++i)check(start_mob_ai_factory(mob,fsm,start,bindings,runtime,services));
  check((services.trace.empty()?"-":services.trace)==expected);check(services.fields()==output);std::string expected_locks;for(unsigned i=0;i<rounds;++i)expected_locks+="LU";check(locks==expected_locks);check(fsm.variables.size()==1&&fsm.variables.at(7).type==1);++cases;
 }
 check(cases==768);std::cout<<cases<<" original factory startup/compiled Debug traces matched\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
