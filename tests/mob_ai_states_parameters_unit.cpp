#include "eternity/game/mob_ai_states_reset.hpp"
#include <array>
#include <bit>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace eternity::game;
static_assert(sizeof(MobAiParameter)==148);
using Fields=std::array<std::uint32_t,13>;
void check(bool ok){if(!ok)throw std::runtime_error("Original AI parameter/reset mismatch");}
MobAiParameter decode(const std::string& hex){check(hex.size()==296);std::array<unsigned char,148> raw{};for(std::size_t i=0;i<148;++i)raw[i]=static_cast<unsigned char>(std::stoul(hex.substr(i*2,2),nullptr,16));MobAiParameter result(0);std::memcpy(&result,raw.data(),148);return result;}
std::string encode(const MobAiParameter& value){std::array<unsigned char,148> raw{};std::memcpy(raw.data(),&value,148);std::ostringstream out;out<<std::hex<<std::setfill('0');for(auto byte:raw)out<<std::setw(2)<<static_cast<unsigned>(byte);return out.str();}
void decode_variables(MobAiVariables& variables,const std::string& text){std::istringstream in(text);std::string value;while(std::getline(in,value,';')){const auto comma=value.find(',');check(comma!=std::string::npos);variables.emplace(static_cast<std::uint32_t>(std::stoul(value.substr(0,comma))),decode(value.substr(comma+1)));}}
std::string values(const MobAiVariables& variables){std::string result;for(const auto& [key,value]:variables){if(!result.empty())result+=';';result+=std::to_string(key)+','+encode(value);}return result;}
Fields fields(const MobRuntime& mob,const MobAiConfiguredFsm& fsm){return {static_cast<std::uint32_t>(mob.status),static_cast<std::uint32_t>(mob.target_kind),mob.target_id,mob.vision_fixed_user,fsm.current,fsm.start,mob.attacked?1U:0U,mob.in_battle?1U:0U,mob.attack_at_moment?1U:0U,mob.attack_enabled[0]?1U:0U,mob.attack_enabled[1]?1U:0U,mob.attack_enabled[2]?1U:0U,mob.attack_enabled[3]?1U:0U};}
void assign(MobRuntime& mob,MobAiConfiguredFsm& fsm,const Fields& f){mob.status=std::bit_cast<std::int32_t>(f[0]);mob.target_kind=std::bit_cast<std::int32_t>(f[1]);mob.target_id=f[2];mob.vision_fixed_user=f[3];fsm.current=f[4];fsm.start=f[5];mob.attacked=f[6]!=0;mob.in_battle=f[7]!=0;mob.attack_at_moment=f[8]!=0;for(unsigned i=0;i<4;++i)mob.attack_enabled[i]=f[9+i]!=0;}
struct Services:MobAiConfiguredServices {
 unsigned mutation=0;std::string trace="-";
 std::int32_t travel_condition(MobRuntime&,MobAiConfiguredFsm&,MobAiConfiguredCondition&) override {throw std::runtime_error("Unexpected condition");}
 void enter(MobRuntime& mob,MobAiConfiguredFsm& fsm){
    trace="E:";bool first=true;for(auto value:fields(mob,fsm)){if(!first)trace+=',';trace+=std::to_string(value);first=false;}trace+=':';trace+=values(fsm.variables);
    if(mutation==1){fsm.current=30;mob.target_id=88;mob.status=17;mob.attack_enabled.fill(false);}
    if(mutation==2){fsm.start=30;fsm.current=0xffffffff;}
    if(mutation==3){auto& value=fsm.variables.at(2);value.type=4;value.text[0]='Z';}
 }
};
MobAiVariableCatalog catalog(std::uint32_t count){MobAiVariableCatalog result(4);constexpr std::array<std::uint32_t,8> ids{1,2,2,0xffffffff,0x80000000,7,123,1};for(unsigned index=0;index<4;++index){if(index==1)continue;MobAiVariableDefinition definition;definition.count=std::bit_cast<std::int32_t>(count);for(auto id:ids){MobAiParameter parameter(4);parameter.variable_id=id;parameter.integer=12345;parameter.text.fill('Q');definition.locals.push_back(parameter);}result[index]=std::move(definition);}return result;}
int main(int argc,char**argv){try{
 check(argc==2);std::ifstream file(argv[1]);check(file.good());std::string line;unsigned cases=0,constructors=0;
 while(std::getline(file,line)){
    if(line.empty()||line[0]=='#')continue;std::istringstream row(line);char kind;row>>kind;
    if(kind=='P'){unsigned id;std::string input,expected;row>>id>>input>>expected;auto value=decode(input);initialize_mob_ai_parameter(value);check(encode(value)==expected);++constructors;continue;}
    unsigned id,operation,ai,mutation;std::uint32_t count;Fields input{},expected{};std::string initial,trace,final;
    row>>id>>operation>>ai>>count>>mutation;for(auto& value:input)row>>value;row>>initial>>trace;for(auto& value:expected)row>>value;row>>final;check(!row.fail());
    MobRuntime mob;MobRuntimeTemplate definition;definition.ai=static_cast<std::uint8_t>(ai);mob.definition=&definition;MobAiConfiguredFsm fsm(input[4],input[5]);assign(mob,fsm,input);decode_variables(fsm.variables,initial);auto definitions=catalog(count);Services services;services.mutation=mutation;
    for(auto code:{10U,20U,0x80000000U})fsm.states[code].enter=[&](MobRuntime& subject,MobAiConfiguredFsm& owner){services.enter(subject,owner);};
    if(operation==0)clear_mob_ai_variables(fsm.variables,definitions,static_cast<std::int32_t>(ai));else if(operation==1)reset_mob_ai_configured(mob,fsm,definitions,services);else reset_mob_ai_user(mob);
    if(fields(mob,fsm)!=expected||values(fsm.variables)!=final||services.trace!=trace)throw std::runtime_error("Case "+std::to_string(id)+" mismatch");++cases;
 }
 check(cases==768&&constructors==64);
 // The image's missing-map insertion leaves its type/text uninitialized.
 // Enforce a typed factory admission contract rather than normalize those bytes.
 MobAiVariables absent;auto definitions=catalog(1);bool failed=false;try{clear_mob_ai_variables(absent,definitions,0);}catch(const std::logic_error&){failed=true;}check(failed&&absent.empty());
 std::cout<<cases<<" original variable/reset cases and "<<constructors<<" retained constructor comparisons passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
