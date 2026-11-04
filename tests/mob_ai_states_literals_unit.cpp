#include "eternity/game/mob_ai_states_literals.hpp"
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
void check(bool ok){if(!ok)throw std::runtime_error("Original literal VM mismatch case "+std::to_string(current_case));}
std::string decode(const std::string& hex){std::string out;for(std::size_t i=0;i<hex.size();i+=2)out+=static_cast<char>(std::stoul(hex.substr(i,2),nullptr,16));return out;}
std::vector<std::string> split(const std::string& text){std::vector<std::string> result;if(text=="-")return result;std::istringstream in(text);std::string item;while(std::getline(in,item,';'))result.push_back(item);return result;}
std::string snapshot(const MobAiValueStack& stack){std::ostringstream out;out<<std::hex<<std::setfill('0');bool first=true;for(const auto& slot:stack.slots){if(!first)out<<';';first=false;const auto* bytes=reinterpret_cast<const unsigned char*>(&slot);for(std::size_t i=0;i<148;++i)out<<std::setw(2)<<static_cast<unsigned>(bytes[i]);}return first?"-":out.str();}
int main(int argc,char**argv){try{
 check(argc==2);std::ifstream file(argv[1]);check(file.good());std::string line;unsigned cases=0,constructors=0;
 while(std::getline(file,line)){
  if(line.empty()||line[0]=='#')continue;if(line[0]=='S'){std::istringstream row(line);char kind;unsigned capacity,top,numeric,unknown;row>>kind>>capacity>>top>>numeric>>unknown;MobAiValueStack stack;check(capacity==64&&top==0xffffffff&&numeric==1&&unknown==1&&stack.capacity==64&&stack.top==-1&&stack.slots.size()==64);for(const auto& value:stack.slots)check(value.integer==0&&value.real==0&&value.dword==0&&value.variable_id==0);++constructors;continue;}std::istringstream row(line);unsigned count,capacity,declared,rounds,cursor,top;std::string tokens,initial,expected;row>>current_case>>count>>capacity>>declared>>rounds>>tokens>>initial>>cursor>>top>>expected;check(!row.fail());MobAiConfiguredFsm owner(1,2);owner.value_stack.capacity=static_cast<std::int32_t>(capacity);owner.value_stack.top=99;owner.value_stack.slots.clear();
  MobAiConfiguredCondition condition;condition.words={current_case%4,declared,0x12345678,0,0,0,0};for(const auto& hex:split(tokens)){auto raw=decode(hex);check(raw.size()==288);MobAiToken token;std::memcpy(&token,raw.data(),288);condition.tokens.push_back(token);}check(condition.tokens.size()==count);
  for(const auto& hex:split(initial)){auto raw=decode(hex);check(raw.size()==148);MobAiParameter slot(0);std::memcpy(&slot,raw.data(),148);owner.value_stack.slots.push_back(slot);}
  MobAiLiteralConditionServices services;MobRuntime subject;for(unsigned i=0;i<rounds;++i)check(services.travel_condition(subject,owner,condition)==0);
  check(condition.words[2]==cursor&&static_cast<std::uint32_t>(owner.value_stack.top)==top&&snapshot(owner.value_stack)==expected&&owner.current==1&&owner.start==2);++cases;
 }
 check(cases==1536&&constructors==1);
 // The limited provider must not manufacture results for unrecovered VM kinds.
 for(int kind:{3,4,8,9}){MobAiConfiguredFsm owner(1,2);MobAiConfiguredCondition condition;condition.words[1]=1;MobAiToken token;token.type=kind;condition.tokens.push_back(token);bool threw=false;try{interpret_mob_ai_literals(owner,condition);}catch(const std::logic_error&){threw=true;}check(threw&&condition.words[2]==1&&owner.value_stack.top==-1);}
 MobAiConfiguredFsm owner(1,2);owner.value_stack.capacity=1;owner.value_stack.slots.emplace_back(0);MobAiConfiguredCondition condition;condition.words[1]=1;MobAiToken token;token.type=2;token.number_type=4;condition.tokens.push_back(token);bool threw=false;try{interpret_mob_ai_literals(owner,condition);}catch(const std::logic_error&){threw=true;}check(threw&&owner.value_stack.top==-1);
 // Phase dispatch must use the actual borrowed condition owner's retained stack.
 MobAiConfiguredFsm containing(1,1),borrowed(2,2);borrowed.value_stack.capacity=1;borrowed.value_stack.slots.emplace_back(0);auto& state=containing.states[1];state.condition_owner=&borrowed;state.enter_program.enabled=true;state.enter_program.count=1;MobAiConfiguredCondition expression;expression.words[1]=1;token.number_type=1;token.integer=123;expression.tokens.push_back(token);state.enter_program.expressions.push_back(expression);MobAiLiteralConditionServices services;MobRuntime subject;start_mob_ai_configured(subject,containing,services);check(borrowed.value_stack.top==0&&borrowed.value_stack.slots[0].integer==123&&containing.value_stack.top==-1&&borrowed.current==2);
 std::cout<<cases<<" original literal VM paths matched\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
