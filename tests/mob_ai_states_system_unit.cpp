#include "eternity/game/mob_ai_states_system.hpp"
#include "eternity/game/mob_ai_states_bindings.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cfenv>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
using namespace eternity::game;
unsigned current_case=0;
void check(bool ok){if(!ok)throw std::runtime_error("Original system call VM mismatch case "+std::to_string(current_case));}
std::string decode(const std::string& hex){std::string out;for(std::size_t i=0;i<hex.size();i+=2)out+=static_cast<char>(std::stoul(hex.substr(i,2),nullptr,16));return out;}
std::vector<std::string> split(const std::string& text){std::vector<std::string> result;if(text=="-")return result;std::istringstream in(text);std::string item;while(std::getline(in,item,';'))result.push_back(item);return result;}
std::string snapshot(const MobAiValueStack& stack){std::ostringstream out;out<<std::hex<<std::setfill('0');bool first=true;for(const auto& slot:stack.slots){if(!first)out<<';';first=false;const auto* bytes=reinterpret_cast<const unsigned char*>(&slot);for(std::size_t i=0;i<148;++i)out<<std::setw(2)<<static_cast<unsigned>(bytes[i]);}return first?"-":out.str();}
std::string variable_snapshot(const MobAiVariables& variables){std::ostringstream out;out<<std::setfill('0');bool first=true;for(const auto& [key,value]:variables){if(!first)out<<';';first=false;out<<std::dec<<key<<','<<std::hex;const auto* bytes=reinterpret_cast<const unsigned char*>(&value);for(std::size_t i=0;i<148;++i)out<<std::setw(2)<<static_cast<unsigned>(bytes[i]);}return out.str();}
std::vector<std::uint32_t> numbers(std::string text){std::replace(text.begin(),text.end(),',',' ');std::istringstream in(text);std::uint32_t value;std::vector<std::uint32_t> out;while(in>>value)out.push_back(value);return out;}
void subject(MobRuntime& mob,const std::vector<std::uint32_t>& v){check(v.size()==7);mob.in_battle=v[0];mob.attacked=v[1];mob.attack_at_moment=v[2];mob.attacked_count=std::bit_cast<std::int32_t>(v[3]);mob.current_time=v[4];mob.target_kind=std::bit_cast<std::int32_t>(v[5]);mob.target_id=v[6];}
std::vector<std::uint32_t> subject(const MobRuntime& mob){return {mob.in_battle,mob.attacked,mob.attack_at_moment,static_cast<std::uint32_t>(mob.attacked_count),mob.current_time,static_cast<std::uint32_t>(mob.target_kind),mob.target_id};}
int main(int argc,char**argv){try{
 check(argc==2);std::ifstream file(argv[1]);check(file.good());std::string line;unsigned cases=0;
 const std::array<std::uint32_t,6> ids{0x0ff3a2fd,0x36824cb7,0x368302a7,0xa45c668b,0xaec1b010,0xbbfc6d86};
 while(std::getline(file,line)){
  if(line.empty()||line[0]=='#')continue;std::istringstream row(line);unsigned count,capacity,declared,rounds,arity,cursor,top,frees;std::string tokens,initial,before,after,expected,results;
  row>>current_case>>count>>capacity>>declared>>rounds>>arity>>tokens>>initial>>before>>after>>cursor>>top>>expected>>results>>frees;check(!row.fail()&&frees==8);
  MobAiConfiguredFsm owner(1,2);owner.value_stack.capacity=static_cast<std::int32_t>(capacity);owner.value_stack.top=99;owner.value_stack.slots.clear();MobAiConfiguredCondition condition;condition.words={current_case%4,declared,0x12345678,0,0,0,0};
  for(const auto& hex:split(tokens)){auto raw=decode(hex);check(raw.size()==288);MobAiToken token;std::memcpy(&token,raw.data(),288);condition.tokens.push_back(token);}check(condition.tokens.size()==count);
  for(const auto& hex:split(initial)){auto raw=decode(hex);check(raw.size()==148);MobAiParameter slot(0);std::memcpy(&slot,raw.data(),148);owner.value_stack.slots.push_back(slot);}
  MobRuntime mob;subject(mob,numbers(before));mob.hp=987;mob.battle_type=27;MobAiSystemCatalog catalog;for(auto id:ids)catalog.emplace(id,MobAiSystemDefinition{std::bit_cast<std::int32_t>(arity)});MobAiMobQueries dispatch(owner,mob);MobAiZeroArgumentServices services(catalog,dispatch);auto expected_results=numbers(results);check(expected_results.size()==rounds);
  for(unsigned i=0;i<rounds;++i)check(services.travel_condition(mob,owner,condition)==std::bit_cast<std::int32_t>(expected_results[i]));
  check(condition.words[2]==cursor&&static_cast<std::uint32_t>(owner.value_stack.top)==top&&snapshot(owner.value_stack)==expected&&subject(mob)==numbers(after)&&owner.variables.empty()&&owner.current==1&&owner.start==2&&mob.hp==987&&mob.battle_type==27&&catalog.size()==6);for(const auto& [id,definition]:catalog)check(definition.parameter_count==std::bit_cast<std::int32_t>(arity));++cases;
 }
 check(cases==768);
 // Real query return drives a configured transition and consumes the live flag.
 {MobAiConfiguredFsm vm(1,1);MobRuntime mob;mob.attack_at_moment=true;MobAiSystemCatalog catalog;catalog.emplace(0xaec1b010U,MobAiSystemDefinition{});MobAiMobQueries dispatch(vm,mob);MobAiZeroArgumentServices services(catalog,dispatch);MobAiToken call;call.type=4;call.function_id=0xaec1b010U;MobAiToken one;one.type=2;one.number_type=1;one.integer=1;MobAiToken equal;equal.type=9;equal.opcode=4;MobAiConfiguredCondition condition;condition.words[1]=3;condition.tokens={call,one,equal};vm.states[1].transitions.push_back({2,1,{condition}});vm.states[2].enter=[&](MobRuntime&,MobAiConfiguredFsm& owner){check(owner.current==1&&!mob.attack_at_moment);mob.target_id=77;};evaluate_mob_ai_configured(mob,vm,services);check(vm.current==2&&!mob.attack_at_moment&&mob.target_id==77);}
 // Metadata is required; a missing entry is never invented as a no-op.
 for(unsigned scenario=0;scenario<3;++scenario){MobAiConfiguredFsm vm(1,1);MobRuntime mob;mob.attack_at_moment=true;MobAiSystemCatalog catalog;const auto id=scenario==2?123U:0xaec1b010U;if(scenario)catalog.emplace(id,MobAiSystemDefinition{scenario==1?1:0});MobAiMobQueries dispatch(vm,mob);MobAiToken call;call.type=4;call.function_id=id;call.param_count=0;MobAiConfiguredCondition condition;condition.words[1]=1;condition.tokens={call};bool failed=false;try{interpret_mob_ai_zero_argument_calls(vm,condition,catalog,dispatch);}catch(const std::logic_error&){failed=true;}check(failed&&mob.attack_at_moment&&vm.value_stack.top==-1&&condition.words[2]==1);}
 // A zero-argument dispatch callback can change the live token ID/metadata.
 {struct Dispatch final:MobAiSystemDispatcher{MobAiConfiguredCondition& condition;explicit Dispatch(MobAiConfiguredCondition& c):condition(c){}MobAiParameter invoke(MobAiConfiguredFsm& owner,std::uint32_t id) override{check(id==1);condition.tokens[0].function_id=2;owner.value_stack.top=3;MobAiParameter value(1);value.integer=42;return value;}};
  for(bool positive:{false,true}){MobAiConfiguredFsm vm(1,1);MobAiConfiguredCondition condition;condition.words[1]=1;MobAiToken call;call.type=4;call.function_id=1;condition.tokens={call};MobAiSystemCatalog catalog;catalog.emplace(1,MobAiSystemDefinition{});catalog.emplace(2,MobAiSystemDefinition{positive?1:0});Dispatch dispatch(condition);bool failed=false;try{check(interpret_mob_ai_zero_argument_calls(vm,condition,catalog,dispatch)==0);}catch(const std::logic_error&){failed=true;}check(failed==positive&&vm.value_stack.top==(positive?3:4));if(!positive)check(vm.value_stack.slots[4].integer==42);}}
 {MobAiConfiguredFsm bound(1,1),other(2,2);MobRuntime mob;MobAiMobQueries dispatch(bound,mob);bool failed=false;try{dispatch.invoke(other,0x0ff3a2fd);}catch(const std::logic_error&){failed=true;}check(failed);}
 std::cout<<cases<<" original system call VM sequences matched\n";
 }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
