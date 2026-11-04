#include "eternity/game/mob_ai_states_admission.hpp"
#include <array>
#include <bit>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <source_location>
#include <stdexcept>
using namespace eternity::game;
unsigned current_case=0;std::uint32_t last_action=0;
void check(bool ok,std::source_location where=std::source_location::current()){if(!ok)throw std::runtime_error("Original state admission mismatch case "+std::to_string(current_case)+" line "+std::to_string(where.line()));}
std::string decode(const std::string& hex){if(hex=="-")return {};std::string out;for(std::size_t i=0;i<hex.size();i+=2)out+=static_cast<char>(std::stoul(hex.substr(i,2),nullptr,16));return out;}
std::string encode(std::string_view text){if(text.empty())return "-";std::ostringstream out;out<<std::hex<<std::setfill('0');for(unsigned char byte:text)out<<std::setw(2)<<static_cast<unsigned>(byte);return out.str();}
std::vector<MobAiConfiguredCondition> read_conditions(std::istream& in){unsigned count;in>>count;check(!in.fail());std::vector<MobAiConfiguredCondition> result(count);for(auto& c:result)for(unsigned i=0;i<3;++i)in>>c.words[i];return result;}
MobAiConfiguredState read_state(std::istream& in){
 MobAiConfiguredState state;std::string text;std::array<std::uint32_t,3> actions{};in>>text>>state.state_code;state.name=decode(text);for(auto& id:actions)in>>id;
 std::array<MobAiConfiguredAction*,3> slots{&state.enter,&state.update,&state.exit};for(unsigned i=0;i<3;++i)if(actions[i])*slots[i]=[id=actions[i]](MobRuntime&,MobAiConfiguredFsm&){last_action=id;};
 for(auto* program:{&state.enter_program,&state.update_program,&state.exit_program}){unsigned enabled;std::uint32_t count;in>>text>>enabled>>count;program->text=decode(text);program->enabled=enabled!=0;program->count=std::bit_cast<std::int32_t>(count);program->expressions=read_conditions(in);}
 unsigned count;in>>count;check(!in.fail());state.transitions.resize(count);for(auto& transition:state.transitions){std::uint32_t id,calc,sum;in>>id>>calc>>sum>>transition.target;transition.id=std::bit_cast<std::int32_t>(id);transition.calc_count=std::bit_cast<std::int32_t>(calc);transition.expected_sum=std::bit_cast<std::int32_t>(sum);in>>text;transition.start_text=decode(text);in>>text;transition.end_text=decode(text);in>>text;transition.output_text=decode(text);transition.conditions=read_conditions(in);}check(!in.fail());return state;
}
void conditions(std::ostream& out,const std::vector<MobAiConfiguredCondition>& list){out<<' '<<list.size();for(const auto& c:list){for(unsigned i=0;i<3;++i)out<<' '<<c.words[i];for(unsigned i=3;i<7;++i)check(c.words[i]==0);}}
std::string state_text(const MobAiConfiguredState& state,MobAiConfiguredFsm& owner){
 std::ostringstream out;out<<encode(state.name)<<' '<<state.state_code;MobRuntime subject;
 for(const auto* action:{&state.enter,&state.update,&state.exit}){last_action=0;if(*action)(*action)(subject,owner);out<<' '<<last_action;}
 for(const auto* program:{&state.enter_program,&state.update_program,&state.exit_program}){out<<' '<<encode(program->text)<<' '<<program->enabled<<' '<<static_cast<std::uint32_t>(program->count);conditions(out,program->expressions);}
 out<<' '<<state.transitions.size();for(const auto& t:state.transitions){out<<' '<<static_cast<std::uint32_t>(t.id)<<' '<<static_cast<std::uint32_t>(t.calc_count)<<' '<<static_cast<std::uint32_t>(t.expected_sum)<<' '<<t.target<<' '<<encode(t.start_text)<<' '<<encode(t.end_text)<<' '<<encode(t.output_text);conditions(out,t.conditions);}return out.str();
}
int main(int argc,char**argv){try{
 check(argc==2);std::ifstream file(argv[1]);check(file.good());std::string line;unsigned cases=0;std::uint64_t frees=0;
 while(std::getline(file,line)){
  if(line.empty()||line[0]=='#')continue;std::istringstream row(line);unsigned local_owner;row>>current_case>>local_owner;MobAiConfiguredFsm owner(222,111),foreign(777,888);auto* condition_owner=local_owner?&owner:&foreign;
  for(unsigned i=0;i<3;++i){auto state=read_state(row);state.condition_owner=condition_owner;admit_mob_ai_state(owner,state);state={};}
  unsigned count;row>>count;check(count==owner.states.size());auto at=owner.states.begin();for(unsigned i=0;i<count;++i,++at){std::uint32_t code;row>>code;auto expected=read_state(row);check(code==at->first&&at->second.condition_owner==condition_owner);check(state_text(expected,owner)==state_text(at->second,owner));}
  unsigned freed;row>>freed;check(!row.fail()&&freed>0&&owner.current==222&&owner.start==111);frees+=freed;++cases;
 }
 struct Observe:MobAiConfiguredServices {MobAiConfiguredFsm* expected=nullptr;unsigned calls=0;std::int32_t travel_condition(MobRuntime&,MobAiConfiguredFsm& owner,MobAiConfiguredCondition&) override {check(&owner==expected);++calls;return 0;}} observe;
 MobAiConfiguredFsm containing(1,1),borrowed(2,2);MobAiConfiguredState state;state.name="Test";state.enter_program.enabled=true;state.enter_program.count=1;state.enter_program.expressions.resize(1);state.condition_owner=&borrowed;admit_mob_ai_state(containing,state);containing.start=mob_ai_name_code("Test");MobRuntime subject;observe.expected=&borrowed;start_mob_ai_configured(subject,containing,observe);check(observe.calls==1&&containing.current==containing.start&&borrowed.current==2);
 check(cases==512);std::cout<<cases<<" original state admissions matched; "<<frees<<" original container frees\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
