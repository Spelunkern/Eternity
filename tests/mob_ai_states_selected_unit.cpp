#include "eternity/game/mob_ai_states_selected.hpp"
#include "eternity/game/zone_mobs.hpp"
#include <array>
#include <bit>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace eternity::game;
using Fields=std::array<std::uint32_t,10>;
unsigned current_case=0;
void check(bool ok){if(!ok)throw std::runtime_error("Original selected movement mismatch case "+std::to_string(current_case));}
UserPosition position(const Fields& fields,unsigned index){return {std::bit_cast<float>(fields[index]),std::bit_cast<float>(fields[index+1]),std::bit_cast<float>(fields[index+2])};}
Fields fields(const MobRuntime& mob){return {static_cast<std::uint32_t>(mob.status),std::bit_cast<std::uint32_t>(mob.position.x),std::bit_cast<std::uint32_t>(mob.position.y),std::bit_cast<std::uint32_t>(mob.position.z),std::bit_cast<std::uint32_t>(mob.respawn_position.x),std::bit_cast<std::uint32_t>(mob.respawn_position.y),std::bit_cast<std::uint32_t>(mob.respawn_position.z),std::bit_cast<std::uint32_t>(mob.selected_position.x),std::bit_cast<std::uint32_t>(mob.selected_position.y),std::bit_cast<std::uint32_t>(mob.selected_position.z)};}
struct Services:MobAiSelectedServices,MobAiCompiledServices,MobAiConfiguredServices,ZoneMobServices {
 unsigned mutation=0;bool success=false;std::string trace="-";
 void move(MobRuntime& mob,UserPosition value,std::int32_t mode) override {trace="M:"+std::to_string(std::bit_cast<std::uint32_t>(value.x))+','+std::to_string(std::bit_cast<std::uint32_t>(value.y))+','+std::to_string(std::bit_cast<std::uint32_t>(value.z))+','+std::to_string(mode);if(success)mob.position=value;if(mutation==1){mob.selected_position={7,17,9};mob.respawn_position={4,5,6};}if(mutation==2){mob.position={11,5,12};mob.status=18;}}
 void invoke(MobAiCompiledFunction,MobRuntime&,MobAiConfiguredFsm&) override {throw std::runtime_error("Unexpected other compiled handler");}
 std::int32_t travel_condition(MobRuntime&,MobAiConfiguredFsm&,MobAiConfiguredCondition&) override {throw std::runtime_error("Unexpected condition");}
 std::uint32_t tick_ms() override {throw std::runtime_error("Unexpected clock");}
 void send_cell_view(CellCoordinates,const eternity::net::Packet&) override {throw std::runtime_error("Unexpected broadcast");}
 std::unique_ptr<MobBroadcast> allocate_broadcast(const eternity::net::Packet&,std::optional<std::uint8_t>) override {throw std::runtime_error("Unexpected buffer allocation");}
 void lua_init(MobRuntime&) override {throw std::runtime_error("Unexpected Lua");}
};
int main(int argc,char**argv){try{
 check(argc==2);std::ifstream file(argv[1]);check(file.good());std::array<MapDefinition,4> maps;std::string line;unsigned cases=0;
 while(std::getline(file,line)){
  if(line.empty()||line[0]=='#')continue;std::istringstream row(line);char type;row>>type;
  if(type=='N'){unsigned index;std::string hex;row>>index>>maps[index].size>>hex;for(std::size_t i=0;i<hex.size();i+=2)maps[index].tiles.push_back(static_cast<std::uint8_t>(std::stoul(hex.substr(i,2),nullptr,16)));continue;}
  unsigned map_id,zone,battle,trail,stack_mode,stack_code,mutation,success;Fields initial{},expected{};std::string trace;row>>current_case>>map_id>>zone>>battle>>trail>>stack_mode>>stack_code>>mutation>>success;for(auto& x:initial)row>>x;row>>trace;for(auto& x:expected)row>>x;check(!row.fail());
  MobRuntime mob;Services services;MapGeometry geometry(32);ZoneMobs world(geometry,services);mob.zone=zone?&world:nullptr;mob.status=std::bit_cast<std::int32_t>(initial[0]);mob.position=position(initial,1);mob.respawn_position=position(initial,4);mob.selected_position=position(initial,7);mob.in_battle=battle!=0;
  if(stack_mode)mob.state_stack=stack_mode==1?std::vector<std::int32_t>{}:std::vector<std::int32_t>{static_cast<std::int32_t>(stack_code)};
  if(trail)mob.trail.add(maps[map_id],{1,1});const auto retained_trail=mob.trail.current();const auto retained_stack=mob.state_stack;
  services.mutation=mutation;services.success=success!=0;MobAiConfiguredFsm fsm(mob_ai_name_code("Selected"),0);MobAiSelectedCompiledServices compiled(maps[map_id],services,services);const std::array<MobAiCompiledStateNames,1> names{{{"Selected","","MoveAtSelectedPosition",""}}};connect_mob_ai_functions(mob,fsm,names,compiled);execute_mob_ai_configured(mob,fsm,services);
  check(services.trace==trace);check(fields(mob)==expected);const auto after=mob.trail.current();check(after.has_value()==retained_trail.has_value());if(after)check(after->x==retained_trail->x&&after->z==retained_trail->z);check(mob.state_stack==retained_stack);mob.zone=nullptr;++cases;
 }
 check(cases==1536);std::cout<<cases<<" original selected-movement traces matched\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
