#include "eternity/game/mob_ai_states_numeric_assignment.hpp"
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
void check(bool ok){if(!ok)throw std::runtime_error("Original numeric assignment VM mismatch case "+std::to_string(current_case));}
std::string decode(const std::string& hex){std::string out;for(std::size_t i=0;i<hex.size();i+=2)out+=static_cast<char>(std::stoul(hex.substr(i,2),nullptr,16));return out;}
std::vector<std::string> split(const std::string& text){std::vector<std::string> result;if(text=="-")return result;std::istringstream in(text);std::string item;while(std::getline(in,item,';'))result.push_back(item);return result;}
std::string snapshot(const MobAiValueStack& stack){std::ostringstream out;out<<std::hex<<std::setfill('0');bool first=true;for(const auto& slot:stack.slots){if(!first)out<<';';first=false;const auto* bytes=reinterpret_cast<const unsigned char*>(&slot);for(std::size_t i=0;i<148;++i)out<<std::setw(2)<<static_cast<unsigned>(bytes[i]);}return first?"-":out.str();}
std::string variable_snapshot(const MobAiVariables& variables){std::ostringstream out;out<<std::setfill('0');bool first=true;for(const auto& [key,value]:variables){if(!first)out<<';';first=false;out<<std::dec<<key<<','<<std::hex;const auto* bytes=reinterpret_cast<const unsigned char*>(&value);for(std::size_t i=0;i<148;++i)out<<std::setw(2)<<static_cast<unsigned>(bytes[i]);}return out.str();}
int main(int argc,char**argv){try{
 check(argc==2);std::ifstream file(argv[1]);check(file.good());std::string line;unsigned cases=0;
 while(std::getline(file,line)){
  if(line.empty()||line[0]=='#')continue;std::istringstream row(line);unsigned count,capacity,declared,rounds,cursor,top,result,frees;std::string tokens,variables,initial,expected,final_variables;row>>current_case>>count>>capacity>>declared>>rounds>>tokens>>variables>>initial>>cursor>>top>>expected>>result>>frees>>final_variables;check(!row.fail()&&frees>0);MobAiConfiguredFsm owner(1,2);owner.value_stack.capacity=static_cast<std::int32_t>(capacity);owner.value_stack.top=99;owner.value_stack.slots.clear();
  MobAiConfiguredCondition condition;condition.words={current_case%4,declared,0x12345678,0,0,0,0};for(const auto& hex:split(tokens)){auto raw=decode(hex);check(raw.size()==288);MobAiToken token;std::memcpy(&token,raw.data(),288);condition.tokens.push_back(token);}check(condition.tokens.size()==count);
  for(const auto& hex:split(initial)){auto raw=decode(hex);check(raw.size()==148);MobAiParameter slot(0);std::memcpy(&slot,raw.data(),148);owner.value_stack.slots.push_back(slot);}
  for(const auto& item:split(variables)){const auto comma=item.find(',');const auto key=static_cast<std::uint32_t>(std::stoul(item.substr(0,comma)));auto raw=decode(item.substr(comma+1));check(raw.size()==148);MobAiParameter value(0);std::memcpy(&value,raw.data(),148);owner.variables.emplace(key,value);}
  const auto retained_variables=variable_snapshot(owner.variables);
  MobAiNumericAssignmentServices services;MobRuntime subject;for(unsigned i=0;i<rounds;++i)check(services.travel_condition(subject,owner,condition)==std::bit_cast<std::int32_t>(result));
  check(condition.words[2]==cursor&&static_cast<std::uint32_t>(owner.value_stack.top)==top&&snapshot(owner.value_stack)==expected&&variable_snapshot(owner.variables)==final_variables&&retained_variables==variables&&owner.current==1&&owner.start==2);++cases;
 }
 check(cases==2304);
 // The limited provider must not manufacture results for unrecovered VM kinds.
 for(int kind:{4}){MobAiConfiguredFsm owner(1,2);MobAiConfiguredCondition condition;condition.words[1]=1;MobAiToken token;token.type=kind;condition.tokens.push_back(token);bool threw=false;try{interpret_mob_ai_numeric_assignment(owner,condition);}catch(const std::logic_error&){threw=true;}check(threw&&condition.words[2]==1&&owner.value_stack.top==-1);}
 MobAiConfiguredFsm owner(1,2);owner.value_stack.capacity=1;owner.value_stack.slots.emplace_back(0);MobAiConfiguredCondition condition;condition.words[1]=1;MobAiToken token;token.type=2;token.number_type=4;condition.tokens.push_back(token);bool threw=false;try{interpret_mob_ai_numeric_assignment(owner,condition);}catch(const std::logic_error&){threw=true;}check(threw&&owner.value_stack.top==-1);
 // Missing typed entries fail without inserting an indeterminate original value.
 {MobAiConfiguredFsm missing(1,2);MobAiConfiguredCondition condition;condition.words[1]=1;MobAiToken token;token.type=3;std::copy_n("Missing",8,token.text.begin());condition.tokens.push_back(token);bool failed=false;try{interpret_mob_ai_numeric_assignment(missing,condition);}catch(const std::logic_error&){failed=true;}check(failed&&missing.variables.empty()&&missing.value_stack.top==-1&&condition.words[2]==1);}
 // Phase dispatch must use the actual borrowed condition owner's retained stack.
 MobAiConfiguredFsm containing(1,1),borrowed(2,2);borrowed.value_stack.capacity=1;borrowed.value_stack.slots.emplace_back(0);auto& state=containing.states[1];state.condition_owner=&borrowed;state.enter_program.enabled=true;state.enter_program.count=1;MobAiConfiguredCondition expression;expression.words[1]=1;token.type=3;std::copy_n("Foreign",8,token.text.begin());MobAiParameter local(1);local.integer=123;local.variable_id=987;borrowed.variables.emplace(mob_ai_name_code("Foreign"),local);local.integer=999;containing.variables.emplace(mob_ai_name_code("Foreign"),local);expression.tokens.push_back(token);state.enter_program.expressions.push_back(expression);MobAiNumericAssignmentServices services;MobRuntime subject;start_mob_ai_configured(subject,containing,services);check(borrowed.value_stack.top==0&&borrowed.value_stack.slots[0].integer==123&&borrowed.value_stack.slots[0].variable_id==987&&containing.value_stack.top==-1&&borrowed.current==2);
 // Recovered arithmetic feeds actual counted-condition state transitions.
 {MobAiConfiguredFsm vm(1,1);MobAiParameter health(1);health.integer=5;vm.variables.emplace(mob_ai_name_code("HP"),health);MobAiConfiguredCondition test;MobAiToken read;read.type=3;std::copy_n("HP",3,read.text.begin());MobAiToken factor;factor.type=2;factor.number_type=1;factor.integer=3;MobAiToken multiply;multiply.type=8;multiply.opcode=8;MobAiToken limit=factor;limit.integer=20;MobAiToken compare;compare.type=9;compare.opcode=0;test.tokens={read,factor,multiply,limit,compare};test.words[1]=5;vm.states[1].transitions.push_back({2,1,{test}});MobRuntime actor;vm.states[2].enter=[&](MobRuntime& m,MobAiConfiguredFsm& owner){check(owner.current==1);m.target_id=99;};MobAiNumericAssignmentServices evaluator;evaluate_mob_ai_configured(actor,vm,evaluator);check(vm.current==1&&vm.value_stack.top==-1);vm.variables.at(mob_ai_name_code("HP")).integer=10;evaluate_mob_ai_configured(actor,vm,evaluator);check(vm.current==2&&actor.target_id==99&&vm.value_stack.top==-1);}
 {MobAiConfiguredFsm vm(1,1);MobAiConfiguredCondition test;test.words[1]=1;MobAiToken compare;compare.type=9;test.tokens.push_back(compare);bool failed=false;try{interpret_mob_ai_numeric_assignment(vm,test);}catch(const std::logic_error&){failed=true;}check(failed&&vm.value_stack.top==-1&&test.words[2]==1);}
 {MobAiConfiguredFsm vm(1,1);MobAiConfiguredCondition test;test.words[1]=3;MobAiToken integer;integer.type=2;integer.number_type=1;MobAiToken real=integer;real.type=7;real.value[0]=0;MobAiToken compare;compare.type=9;test.tokens={integer,real,compare};bool failed=false;try{interpret_mob_ai_numeric_assignment(vm,test);}catch(const std::logic_error&){failed=true;}check(failed&&vm.value_stack.top==-1&&test.words[2]==3);}
 // Original CPU-fault paths and unrecovered assignments are explicit errors.
 for(unsigned scenario=0;scenario<4;++scenario){MobAiConfiguredFsm vm(1,1);MobAiConfiguredCondition test;test.words[1]=3;MobAiToken left;left.type=2;left.number_type=scenario>=2?2:1;left.integer=scenario==1?std::numeric_limits<std::int32_t>::min():7;left.real=1.5F;MobAiToken right=left;right.number_type=1;right.integer=scenario==0?0:scenario==1?-1:2;MobAiToken operation;operation.type=8;operation.opcode=scenario==2?11:scenario==3?6:9;test.tokens={left,right,operation};bool failed=false;try{interpret_mob_ai_numeric_assignment(vm,test);}catch(const std::logic_error&){failed=true;}check(failed&&vm.value_stack.top==-1&&test.words[2]==3);}
 // Entry assignment writes into the borrowed FSM and a later transition sees it.
 {MobAiConfiguredFsm root(1,1),data(8,8);const auto key=mob_ai_name_code("Value");MobAiParameter value(1);value.variable_id=key;value.integer=5;data.variables.emplace(key,value);
  auto& state=root.states[1];state.condition_owner=&data;state.enter_program.enabled=true;state.enter_program.count=1;
  MobAiToken read;read.type=3;std::copy_n("Value",6,read.text.begin());MobAiToken literal;literal.type=2;literal.number_type=1;literal.number_type=2;literal.real=1.5F;MobAiToken set;set.type=8;set.opcode=10;
  MobAiConfiguredCondition write;write.words[1]=3;write.tokens={read,literal,set};state.enter_program.expressions.push_back(write);
  MobAiToken equal;equal.type=9;equal.opcode=4;MobAiConfiguredCondition test;test.words[1]=3;literal.number_type=1;literal.integer=1;test.tokens={read,literal,equal};data.states[8].transitions.push_back({2,1,{test}});
  MobAiNumericAssignmentServices evaluator;MobRuntime actor;start_mob_ai_configured(actor,root,evaluator);check(data.variables.at(key).integer==1&&data.value_stack.top==0&&root.variables.empty());evaluate_mob_ai_configured(actor,data,evaluator);check(root.current==1&&data.current==2&&data.value_stack.top==-1);}
 // Missing target is rejected after the recovered result push, without inserting.
 {MobAiConfiguredFsm vm(1,1);const auto key=mob_ai_name_code("Source");MobAiParameter local(1);local.variable_id=123;local.integer=7;vm.variables.emplace(key,local);MobAiToken read;read.type=3;std::copy_n("Source",7,read.text.begin());MobAiToken value;value.type=2;value.number_type=1;value.integer=99;MobAiToken set;set.type=8;set.opcode=10;MobAiConfiguredCondition test;test.words[1]=3;test.tokens={read,value,set};bool failed=false;try{interpret_mob_ai_numeric_assignment(vm,test);}catch(const std::logic_error&){failed=true;}check(failed&&vm.value_stack.top==0&&vm.value_stack.slots[0].integer==99&&vm.variables.size()==1&&vm.variables.at(key).integer==7);}
 // Integer-to-float rounding requires the recovered original nearest mode.
 {MobAiConfiguredFsm vm(1,1);const auto key=mob_ai_name_code("Float");MobAiParameter local(2);local.variable_id=key;local.real=9.0F;vm.variables.emplace(key,local);MobAiToken read;read.type=3;std::copy_n("Float",6,read.text.begin());MobAiToken literal;literal.type=2;literal.number_type=1;literal.integer=0x01000001;MobAiToken set;set.type=8;set.opcode=10;MobAiConfiguredCondition test;test.words[1]=3;test.tokens={read,literal,set};const auto rounding=std::fegetround();check(std::fesetround(FE_DOWNWARD)==0);bool failed=false;try{interpret_mob_ai_numeric_assignment(vm,test);}catch(const std::logic_error&){failed=true;}check(std::fesetround(rounding)==0);check(failed&&vm.value_stack.top==-1&&vm.variables.at(key).real==9.0F);}
 std::cout<<cases<<" original numeric assignment VM paths matched\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
