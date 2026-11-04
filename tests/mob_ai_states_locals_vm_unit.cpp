#include "eternity/game/mob_ai_states_locals_vm.hpp"
#include "eternity/game/mob_ai_states_bindings.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace eternity::game;
unsigned current_case=0;
void check(bool ok){if(!ok)throw std::runtime_error("Original local VM mismatch case "+std::to_string(current_case));}
std::string decode(const std::string& hex){std::string out;for(std::size_t i=0;i<hex.size();i+=2)out+=static_cast<char>(std::stoul(hex.substr(i,2),nullptr,16));return out;}
std::vector<std::string> split(const std::string& text){std::vector<std::string> result;if(text=="-")return result;std::istringstream in(text);std::string item;while(std::getline(in,item,';'))result.push_back(item);return result;}
std::string snapshot(const MobAiValueStack& stack){std::ostringstream out;out<<std::hex<<std::setfill('0');bool first=true;for(const auto& slot:stack.slots){if(!first)out<<';';first=false;const auto* bytes=reinterpret_cast<const unsigned char*>(&slot);for(std::size_t i=0;i<148;++i)out<<std::setw(2)<<static_cast<unsigned>(bytes[i]);}return first?"-":out.str();}
std::string variable_snapshot(const MobAiVariables& variables){std::ostringstream out;out<<std::setfill('0');bool first=true;for(const auto& [key,value]:variables){if(!first)out<<';';first=false;out<<std::dec<<key<<','<<std::hex;const auto* bytes=reinterpret_cast<const unsigned char*>(&value);for(std::size_t i=0;i<148;++i)out<<std::setw(2)<<static_cast<unsigned>(bytes[i]);}return out.str();}
int main(int argc,char**argv){try{
 check(argc==2);std::ifstream file(argv[1]);check(file.good());std::string line;unsigned cases=0;
 while(std::getline(file,line)){
  if(line.empty()||line[0]=='#')continue;std::istringstream row(line);unsigned count,capacity,declared,rounds,cursor,top,frees;std::string tokens,variables,initial,expected;row>>current_case>>count>>capacity>>declared>>rounds>>tokens>>variables>>initial>>cursor>>top>>expected>>frees;check(!row.fail()&&frees>0);MobAiConfiguredFsm owner(1,2);owner.value_stack.capacity=static_cast<std::int32_t>(capacity);owner.value_stack.top=99;owner.value_stack.slots.clear();
  MobAiConfiguredCondition condition;condition.words={current_case%4,declared,0x12345678,0,0,0,0};for(const auto& hex:split(tokens)){auto raw=decode(hex);check(raw.size()==288);MobAiToken token;std::memcpy(&token,raw.data(),288);condition.tokens.push_back(token);}check(condition.tokens.size()==count);
  for(const auto& hex:split(initial)){auto raw=decode(hex);check(raw.size()==148);MobAiParameter slot(0);std::memcpy(&slot,raw.data(),148);owner.value_stack.slots.push_back(slot);}
  for(const auto& item:split(variables)){const auto comma=item.find(',');const auto key=static_cast<std::uint32_t>(std::stoul(item.substr(0,comma)));auto raw=decode(item.substr(comma+1));check(raw.size()==148);MobAiParameter value(0);std::memcpy(&value,raw.data(),148);owner.variables.emplace(key,value);}
  const auto retained_variables=variable_snapshot(owner.variables);
  MobAiLocalValueConditionServices services;MobRuntime subject;for(unsigned i=0;i<rounds;++i)check(services.travel_condition(subject,owner,condition)==0);
  check(condition.words[2]==cursor&&static_cast<std::uint32_t>(owner.value_stack.top)==top&&snapshot(owner.value_stack)==expected&&variable_snapshot(owner.variables)==retained_variables&&retained_variables==variables&&owner.current==1&&owner.start==2);++cases;
 }
 check(cases==1024);
 // The limited provider must not manufacture results for unrecovered VM kinds.
 for(int kind:{4,8,9}){MobAiConfiguredFsm owner(1,2);MobAiConfiguredCondition condition;condition.words[1]=1;MobAiToken token;token.type=kind;condition.tokens.push_back(token);bool threw=false;try{interpret_mob_ai_local_values(owner,condition);}catch(const std::logic_error&){threw=true;}check(threw&&condition.words[2]==1&&owner.value_stack.top==-1);}
 MobAiConfiguredFsm owner(1,2);owner.value_stack.capacity=1;owner.value_stack.slots.emplace_back(0);MobAiConfiguredCondition condition;condition.words[1]=1;MobAiToken token;token.type=2;token.number_type=4;condition.tokens.push_back(token);bool threw=false;try{interpret_mob_ai_local_values(owner,condition);}catch(const std::logic_error&){threw=true;}check(threw&&owner.value_stack.top==-1);
 // Missing typed entries fail without inserting an indeterminate original value.
 {MobAiConfiguredFsm missing(1,2);MobAiConfiguredCondition condition;condition.words[1]=1;MobAiToken token;token.type=3;std::copy_n("Missing",8,token.text.begin());condition.tokens.push_back(token);bool failed=false;try{interpret_mob_ai_local_values(missing,condition);}catch(const std::logic_error&){failed=true;}check(failed&&missing.variables.empty()&&missing.value_stack.top==-1&&condition.words[2]==1);}
 // Phase dispatch must use the actual borrowed condition owner's retained stack.
 MobAiConfiguredFsm containing(1,1),borrowed(2,2);borrowed.value_stack.capacity=1;borrowed.value_stack.slots.emplace_back(0);auto& state=containing.states[1];state.condition_owner=&borrowed;state.enter_program.enabled=true;state.enter_program.count=1;MobAiConfiguredCondition expression;expression.words[1]=1;token.type=3;std::copy_n("Foreign",8,token.text.begin());MobAiParameter local(1);local.integer=123;local.variable_id=987;borrowed.variables.emplace(mob_ai_name_code("Foreign"),local);local.integer=999;containing.variables.emplace(mob_ai_name_code("Foreign"),local);expression.tokens.push_back(token);state.enter_program.expressions.push_back(expression);MobAiLocalValueConditionServices services;MobRuntime subject;start_mob_ai_configured(subject,containing,services);check(borrowed.value_stack.top==0&&borrowed.value_stack.slots[0].integer==123&&borrowed.value_stack.slots[0].variable_id==987&&containing.value_stack.top==-1&&borrowed.current==2);
 std::cout<<cases<<" original local-variable VM paths matched\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
