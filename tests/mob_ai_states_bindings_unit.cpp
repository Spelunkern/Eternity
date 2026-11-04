#include "eternity/game/mob_ai_states_bindings.hpp"
#include <array>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace eternity::game;
unsigned current_case=0;
void check(bool ok){if(!ok)throw std::runtime_error("Original compiled binding mismatch case "+std::to_string(current_case));}
std::string decode(const std::string& hex){if(hex=="-")return {};std::string out;for(std::size_t i=0;i<hex.size();i+=2)out+=static_cast<char>(std::stoul(hex.substr(i,2),nullptr,16));return out;}
struct Services:MobAiCompiledServices,MobAiConfiguredServices {
 std::uint32_t called=0;MobRuntime* subject=nullptr;
 void invoke(MobAiCompiledFunction kind,MobRuntime& mob,MobAiConfiguredFsm&) override {
  constexpr std::array<std::uint32_t,6> addresses{0x4afcf0,0x4aff90,0x4b01c0,0x4b0360,0x4b0520,0x4afce0};called=addresses.at(static_cast<unsigned>(kind));subject=&mob;
 }
 std::int32_t travel_condition(MobRuntime&,MobAiConfiguredFsm&,MobAiConfiguredCondition&) override {throw std::runtime_error("Unexpected compiled binding condition");}
};
int main(int argc,char**argv){try{
 check(argc==2);std::ifstream file(argv[1]);check(file.good());std::string line;unsigned cases=0;
 while(std::getline(file,line)){
  if(line.empty()||line[0]=='#')continue;std::istringstream row(line);unsigned count,rounds,size;std::string input,locks,expected;row>>current_case>>count>>rounds>>input>>size>>locks>>expected;check(!row.fail());
  std::vector<MobAiCompiledStateNames> names;std::istringstream rows(input);std::string record;while(std::getline(rows,record,';')){std::istringstream fields(record);std::array<std::string,4> parts;for(auto& p:parts){std::getline(fields,p,',');p=decode(p);}names.push_back({parts[0],parts[1],parts[2],parts[3]});}names.resize(count);
  MobRuntime mob,other;Services services;MobAiConfiguredFsm fsm(22,33);
  const auto custom=[&services](MobRuntime& target,MobAiConfiguredFsm&){services.called=0x11110000;services.subject=&target;};
  fsm.compiled_functions.emplace_back("Custom",custom);
  fsm.compiled_functions.emplace_back("MoveRandom",[&](MobRuntime&,MobAiConfiguredFsm&){services.called=0x22220000;});
  fsm.compiled_functions.emplace_back("Battle",[&](MobRuntime&,MobAiConfiguredFsm&){services.called=0x33330000;});
  for(const auto& n:names){auto& state=fsm.states[mob_ai_name_code(n.state)];state.enter_program.count=17;state.enter_program.expressions.resize(1);state.enter_program.expressions[0].words.fill(99);state.transitions.push_back({44,2,{}});}
  const auto before=fsm.states.size();for(unsigned i=0;i<rounds;++i)connect_mob_ai_functions(mob,fsm,names,services);
  check(fsm.current==22&&fsm.start==33&&fsm.compiled_functions.size()==size&&fsm.states.size()==before);std::string expected_locks;for(unsigned i=0;i<rounds;++i)expected_locks+="LU";check(locks==expected_locks);
  unsigned entries=0;
  if(expected!="-"){std::istringstream states(expected);while(std::getline(states,record,';')){std::istringstream fields(record);std::string item;std::array<std::uint32_t,5> words{};for(auto& word:words){std::getline(fields,item,',');word=static_cast<std::uint32_t>(std::stoul(item));}auto& state=fsm.states.at(words[0]);check(words[4]==1);check(state.enter_program.count==17&&state.enter_program.expressions[0].words[6]==99&&!state.enter_program.enabled&&!state.update_program.enabled&&state.transitions.size()>=1);
   std::array<MobAiConfiguredAction*,3> slots{&state.enter,&state.update,&state.exit};
   for(unsigned phase=0;phase<3;++phase){services.called=0;services.subject=nullptr;mob.target_id=77;mob.attacked=true;mob.in_battle=true;mob.vision_fixed_user=0;other.target_id=88;
    if(phase==0){fsm.start=words[0];start_mob_ai_configured(other,fsm,services);}else if(phase==1)execute_mob_ai_configured(other,fsm,services);else if(*slots[phase])(*slots[phase])(other,fsm);
    if(words[phase+1]==0x4b05b0){check(mob.target_id==0&&!mob.attacked&&!mob.in_battle&&services.called==0);}else{check(services.called==words[phase+1]);if(services.called)check(services.subject==&mob);}
    check(other.target_id==88);
   }
   ++entries;
  }}check(entries==before);++cases;
 }
 check(cases==384);check(mob_ai_name_code("Aa")==mob_ai_name_code("B@"));
 std::cout<<cases<<" original compiled binding traces matched\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
