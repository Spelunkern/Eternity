#include "eternity/game/mob_ai_states_locals.hpp"
#include <array>
#include <bit>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <source_location>
#include <stdexcept>
using namespace eternity::game;
unsigned current_case=0;
void check(bool v,std::source_location where=std::source_location::current()){if(!v)throw std::runtime_error("Original local factory mismatch case "+std::to_string(current_case)+" line "+std::to_string(where.line()));}
MobAiParameter decode(const std::string& hex){check(hex.size()==296);std::array<unsigned char,148> raw{};for(std::size_t i=0;i<148;++i)raw[i]=static_cast<unsigned char>(std::stoul(hex.substr(i*2,2),nullptr,16));MobAiParameter result(0);std::memcpy(&result,raw.data(),148);return result;}
std::string encode(const MobAiParameter& v){std::array<unsigned char,148> raw{};std::memcpy(raw.data(),&v,148);std::ostringstream out;out<<std::hex<<std::setfill('0');for(auto b:raw)out<<std::setw(2)<<static_cast<unsigned>(b);return out.str();}
std::string values(const MobAiVariables& vars){std::string out;for(const auto& [k,v]:vars){if(!out.empty())out+=';';out+=std::to_string(k)+','+encode(v);}return out.empty()?"-":out;}
void parse(MobAiVariables& vars,const std::string& text){if(text=="-")return;std::istringstream in(text);std::string item;while(std::getline(in,item,';')){auto comma=item.find(',');vars.emplace(static_cast<std::uint32_t>(std::stoul(item.substr(0,comma))),decode(item.substr(comma+1)));}}
int main(int argc,char** argv){try{
 check(argc==2);std::ifstream file(argv[1]);check(file.good());std::string line;unsigned cases=0;
 while(std::getline(file,line)){
  if(line.empty()||line[0]=='#')continue;
  std::istringstream row(line);unsigned id,ai,count,success;std::string initial,parameters,expected,trace;row>>id>>ai>>count>>initial>>parameters>>success>>expected>>trace;check(!row.fail());current_case=id;
  MobAiVariableDefinition def;def.count=std::bit_cast<std::int32_t>(count);std::istringstream source(parameters);std::string item;while(std::getline(source,item,';'))def.locals.push_back(decode(item));
  MobAiVariableCatalog catalog(4);catalog[0]=def;catalog[2]=def;catalog[3]=def;
  MobAiVariables actual;parse(actual,initial);check(install_mob_ai_locals(actual,catalog,static_cast<std::int32_t>(ai))==(success!=0));check(values(actual)==expected);
  // Replay every original insertion call against the same native owner and
  // verify unconditional success, first-key retention and complete value copy.
  MobAiVariables replay;parse(replay,initial);if(trace!="-"){std::istringstream calls(trace);while(std::getline(calls,item,';')){auto comma=item.find(',');auto key=static_cast<std::uint32_t>(std::stoul(item.substr(0,comma)));auto parameter=decode(item.substr(comma+1));check(add_mob_ai_local(replay,key,parameter));parameter.text.fill('!');}}
  check(values(replay)==expected);
  if(success){clear_mob_ai_variables(actual,catalog,static_cast<std::int32_t>(ai));check(actual.size()==replay.size());}
  ++cases;
 }
 check(cases==512);
 MobAiVariables empty;MobAiVariableCatalog bad(1);MobAiVariableDefinition def;def.count=1;bad[0]=def;bool threw=false;try{install_mob_ai_locals(empty,bad,0);}catch(const std::logic_error&){threw=true;}check(threw&&empty.empty());
 MobAiParameter invalid(4);invalid.text.fill('X');threw=false;try{add_mob_ai_local(empty,7,invalid);}catch(const std::logic_error&){threw=true;}check(threw&&empty.empty());
 std::cout<<cases<<" original local factory traces matched\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
