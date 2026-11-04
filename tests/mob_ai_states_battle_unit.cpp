#include "eternity/game/mob_ai_states_battle.hpp"
#include "eternity/game/zone_mobs.hpp"
#include <array>
#include <bit>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace eternity::game;
using Fields=std::array<std::uint32_t,15>;
unsigned current_case=0;
void check(bool ok){if(!ok)throw std::runtime_error("Original compiled Battle mismatch case "+std::to_string(current_case));}
struct Services:MobAiBattleServices,MobAiCompiledServices,ZoneMobServices {
 MobRuntime& mob;MobAiConfiguredFsm& fsm;MobAiBattleDefinition& definition;MobAiBattleTarget target;
 unsigned mutation=0;bool present=false,found=false;std::string trace;
 Services(MobRuntime& m,MobAiConfiguredFsm& f,MobAiBattleDefinition& d):mob(m),fsm(f),definition(d){}
 Fields fields(){unsigned mask=0;for(unsigned i=0;i<4;++i)mask|=mob.attack_enabled[i]?1U<<i:0;return {static_cast<std::uint32_t>(mob.status),static_cast<std::uint32_t>(mob.target_kind),mob.target_id,mob.next_attack,mob.current_time,static_cast<std::uint32_t>(mob.sleep),static_cast<std::uint32_t>(mob.stun),mob.attacked?1U:0U,mob.in_battle?1U:0U,mask,fsm.current,fsm.start,static_cast<std::uint32_t>(fsm.variables.at(7).integer),definition.country,static_cast<std::uint32_t>(target.life_status)};}
 void emit(char tag,std::initializer_list<std::uint32_t> extra={}){if(!trace.empty())trace+=';';trace+=tag;trace+=':';bool first=true;const auto append=[&](std::uint32_t value){if(!first)trace+=',';first=false;trace+=std::to_string(value);};for(auto x:extra)append(x);for(auto x:fields())append(x);}
 MobAiBattleTarget* find_user(MobRuntime& m,std::uint32_t id) override {check(&m==&mob);emit('F',{id});if(mutation==1){mob.current_time=333;mob.target_id=99;}if(mutation==2)definition.country=target.country;if(mutation==3)target.life_status=1;return present?&target:nullptr;}
 void remove_vision_fix(MobRuntime&) override {emit('V');if(mutation==4){mob.target_id=88;mob.status=17;}}
 MobAiBattleTarget* find_target(MobRuntime&) override {emit('T');if(mutation==5){mob.next_attack=999;mob.current_time=555;mob.sleep=2;mob.stun=3;mob.status=18;mob.target_id=89;}return found?&target:nullptr;}
 void attack_ai(MobRuntime&,std::uint32_t now,MobAiBattleTarget& value) override {check(&value==&target);emit('A',{now});if(mutation==7){mob.target_id=90;mob.current_time=666;mob.stun=4;}}
 void enter(MobRuntime&){emit('E');if(mutation==6){mob.status=17;mob.target_id=88;fsm.current=77;fsm.variables.at(7).integer=99;}}
 std::int32_t travel_condition(MobRuntime&,MobAiConfiguredFsm&,MobAiConfiguredCondition&) override {throw std::runtime_error("Unexpected condition");}
 void invoke(MobAiCompiledFunction,MobRuntime&,MobAiConfiguredFsm&) override {throw std::runtime_error("Unexpected external compiled handler");}
 std::uint32_t tick_ms() override {throw std::runtime_error("Unexpected zone clock");}
 void send_cell_view(CellCoordinates,const eternity::net::Packet&) override {throw std::runtime_error("Unexpected zone broadcast");}
 std::unique_ptr<MobBroadcast> allocate_broadcast(const eternity::net::Packet&,std::optional<std::uint8_t>) override {throw std::runtime_error("Unexpected broadcast allocation");}
 void lua_init(MobRuntime&) override {throw std::runtime_error("Unexpected Lua initialization");}
};
int main(int argc,char** argv){try{
 check(argc==2);std::ifstream file(argv[1]);check(file.good());std::string line;unsigned cases=0;
 while(std::getline(file,line)){
  if(line.empty()||line[0]=='#')continue;std::istringstream row(line);unsigned operation,mutation,zone,present,found,speed,stop,only_lua,country,immunity,attackable,visible;std::uint32_t now;Fields input{},expected{};std::string trace;
  row>>current_case>>operation>>mutation>>zone>>present>>found>>speed>>stop>>only_lua>>country>>immunity>>attackable>>visible>>now;for(auto& x:input)row>>x;row>>trace;for(auto& x:expected)row>>x;check(!row.fail());
  MobRuntime mob;MobRuntimeTemplate base;mob.definition=&base;MobAiConfiguredFsm fsm(input[10],input[11]);MobAiBattleDefinition definition{input[13]};Services services(mob,fsm,definition);MapGeometry geometry(81);ZoneMobs world(geometry,services);mob.zone=zone?&world:nullptr;
  mob.status=std::bit_cast<std::int32_t>(input[0]);mob.target_kind=std::bit_cast<std::int32_t>(input[1]);mob.target_id=input[2];mob.next_attack=input[3];mob.current_time=input[4];mob.sleep=std::bit_cast<std::int32_t>(input[5]);mob.stun=std::bit_cast<std::int32_t>(input[6]);mob.attacked=input[7]!=0;mob.in_battle=input[8]!=0;for(unsigned i=0;i<4;++i)mob.attack_enabled[i]=(input[9]&(1U<<i))!=0;mob.attack_speed=std::bit_cast<std::int32_t>(speed);mob.stop=static_cast<std::int32_t>(stop);mob.only_lua_attack=static_cast<std::int32_t>(only_lua);
  MobAiParameter variable(1);variable.variable_id=7;variable.integer=std::bit_cast<std::int32_t>(input[12]);fsm.variables.emplace(7,variable);MobAiVariableDefinition locals;locals.count=1;locals.locals.push_back(variable);MobAiVariableCatalog catalog(1);catalog[0]=locals;
  services.target={42,std::bit_cast<std::int32_t>(input[14]),static_cast<std::uint8_t>(country),static_cast<std::uint8_t>(immunity),static_cast<std::uint8_t>(attackable),static_cast<std::uint8_t>(visible)};services.mutation=mutation;services.present=present!=0;services.found=found!=0;fsm.states[10].enter=[&](MobRuntime& m,MobAiConfiguredFsm&){services.enter(m);};
  MobAiBattleCompiledServices compiled(definition,catalog,services,services);const std::array<MobAiCompiledStateNames,1> names{{{"BattleTest","","Battle",""}}};connect_mob_ai_functions(mob,fsm,names,compiled);
  if(operation)attack_mob_ai_battle(mob,fsm,definition,catalog,now,services.target,services);else execute_mob_ai_configured(mob,fsm,services);
  check((services.trace.empty()?"-":services.trace)==trace);check(services.fields()==expected);mob.zone=nullptr;++cases;
 }
 check(cases==1536);std::cout<<cases<<" original compiled Battle/PAI traces matched\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
