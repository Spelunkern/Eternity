#include "eternity/game/mob_ai_states_argument_writes.hpp"
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
void check(bool ok){if(!ok)throw std::runtime_error("Original CalculateParam mismatch case "+std::to_string(current_case));}
std::string decode(const std::string& hex){std::string out;for(std::size_t i=0;i<hex.size();i+=2)out+=static_cast<char>(std::stoul(hex.substr(i,2),nullptr,16));return out;}
std::vector<std::string> split(const std::string& text){std::vector<std::string> result;if(text=="-")return result;std::istringstream in(text);std::string item;while(std::getline(in,item,';'))result.push_back(item);return result;}
std::string snapshot(const MobAiValueStack& stack){std::ostringstream out;out<<std::hex<<std::setfill('0');bool first=true;for(const auto& slot:stack.slots){if(!first)out<<';';first=false;const auto* bytes=reinterpret_cast<const unsigned char*>(&slot);for(std::size_t i=0;i<148;++i)out<<std::setw(2)<<static_cast<unsigned>(bytes[i]);}return first?"-":out.str();}
std::string variable_snapshot(const MobAiVariables& variables){std::ostringstream out;out<<std::setfill('0');bool first=true;for(const auto& [key,value]:variables){if(!first)out<<';';first=false;out<<std::dec<<key<<','<<std::hex;const auto* bytes=reinterpret_cast<const unsigned char*>(&value);for(std::size_t i=0;i<148;++i)out<<std::setw(2)<<static_cast<unsigned>(bytes[i]);}return out.str();}
std::string bytes(const MobAiParameter& value){MobAiValueStack stack;stack.slots={value};return snapshot(stack);}
int main(int argc,char**argv){try{
 check(argc==2);std::ifstream file(argv[1]);check(file.good());std::string line;unsigned cases=0;
 while(std::getline(file,line)){
  if(line.empty()||line[0]=='#')continue;std::istringstream row(line);unsigned count,declared,start,rounds,cursor,frees;std::string tokens,variables,initial,expected,retained,final_variables;
  row>>current_case>>count>>declared>>start>>rounds>>tokens>>variables>>initial>>expected>>cursor>>frees>>retained>>final_variables;check(!row.fail()&&frees==rounds+4);
  MobAiConfiguredFsm owner(1,2);owner.value_stack.capacity=3;owner.value_stack.top=1;owner.value_stack.slots.clear();MobAiConfiguredCondition condition;condition.words={current_case%4,declared,start,0,0,0,0};
  for(const auto& hex:split(tokens)){auto raw=decode(hex);check(raw.size()==288);MobAiToken token;std::memcpy(&token,raw.data(),288);condition.tokens.push_back(token);}check(condition.tokens.size()==count);
  for(const auto& hex:split(retained)){auto raw=decode(hex);check(raw.size()==148);MobAiParameter value(0);std::memcpy(&value,raw.data(),148);owner.value_stack.slots.push_back(value);}
  for(const auto& item:split(variables)){const auto comma=item.find(',');const auto key=static_cast<std::uint32_t>(std::stoul(item.substr(0,comma)));auto raw=decode(item.substr(comma+1));check(raw.size()==148);MobAiParameter value(0);std::memcpy(&value,raw.data(),148);owner.variables.emplace(key,value);}
  MobAiParameter output(0);const auto raw=decode(initial);check(raw.size()==148);std::memcpy(&output,raw.data(),148);const auto outputs=split(expected);check(outputs.size()==rounds);
  for(unsigned i=0;i<rounds;++i){calculate_mob_ai_writing_argument(owner,condition,output);check(bytes(output)==outputs[i]);}
  check(condition.words[2]==cursor&&owner.value_stack.top==1&&owner.value_stack.capacity==3&&snapshot(owner.value_stack)==retained&&variable_snapshot(owner.variables)==final_variables&&owner.current==1&&owner.start==2);++cases;
 }
 check(cases==2048);
 // Shared cursor returns separate arguments; exhausted count does not publish.
 {MobAiConfiguredFsm owner(1,1);MobAiConfiguredCondition condition;MobAiToken number;number.type=2;number.number_type=1;number.integer=42;MobAiToken end;end.type=5;MobAiToken text;text.type=7;std::copy_n("Next",5,text.value.begin());condition.tokens={number,end,text,end,number};condition.words[1]=5;MobAiParameter output(3);output.text.fill('X');calculate_mob_ai_writing_argument(owner,condition,output);check(output.type==1&&output.integer==42&&condition.words[2]==2);calculate_mob_ai_writing_argument(owner,condition,output);check(output.type==4&&output.text[0]=='N'&&output.text[4]==0&&output.text[5]=='X'&&condition.words[2]==4);const auto retained=bytes(output);calculate_mob_ai_writing_argument(owner,condition,output);check(bytes(output)==retained&&condition.words[2]==6&&owner.value_stack.top==-1);}
 for(unsigned scenario=0;scenario<5;++scenario){MobAiConfiguredFsm owner(1,1);MobAiConfiguredCondition condition;condition.words[1]=1;MobAiToken token;token.type=scenario==0?4:scenario==1?8:scenario==2?3:scenario==3?2:7;token.number_type=4;if(scenario==4)token.value.fill('X');condition.tokens={token};MobAiParameter output(1);output.integer=123;const auto before=bytes(output);bool failed=false;try{calculate_mob_ai_writing_argument(owner,condition,output);}catch(const std::logic_error&){failed=true;}check(failed&&bytes(output)==before&&owner.variables.empty()&&owner.value_stack.top==-1&&condition.words[2]==1);}
 // Faulting integer operations and unrecovered argument matrices are explicit.
 for(unsigned scenario=0;scenario<4;++scenario){MobAiConfiguredFsm owner(1,1);MobAiConfiguredCondition condition;condition.words[1]=4;MobAiToken left;left.type=2;left.number_type=scenario>=2?2:1;left.real=1.5F;left.integer=scenario==1?std::numeric_limits<std::int32_t>::min():7;MobAiToken right=left;right.number_type=1;right.integer=scenario==0?0:scenario==1?-1:2;MobAiToken operation;operation.type=8;operation.opcode=scenario==3?10:9;MobAiToken end;end.type=5;condition.tokens={left,right,operation,end};MobAiParameter output(1);output.integer=123;bool failed=false;try{calculate_mob_ai_writing_argument(owner,condition,output);}catch(const std::logic_error&){failed=true;}check(failed&&output.integer==123&&owner.value_stack.top==-1&&condition.words[2]==3);}
 std::cout<<cases<<" original CalculateParam sequences matched\n";
 }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
