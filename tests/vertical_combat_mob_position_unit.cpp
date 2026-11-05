#include "eternity/game/vertical_combat_mob_position.hpp"
#include <array>
#include <bit>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace eternity::game;
namespace {
struct Services:VerticalCombatMobPositionServices {
 std::array<std::byte,268> record{};std::uint32_t secondary=0,now=0;unsigned clocks=0;
 std::uint32_t tick_ms() override{++clocks;return now;}
 std::uint32_t mob_secondary_state(const MobRuntime&) override{return secondary;}
 std::span<const std::byte> mob_definition_record(const MobRuntime&) override{return record;}
 void put(unsigned at,std::uint32_t value){for(unsigned i=0;i<4;++i)record[at+i]=std::byte((value>>(8*i))&255);}
};
}
int main(int argc,char** argv){try{
 if(argc!=2)throw std::runtime_error("fixture required");std::ifstream input(argv[1]);if(!input)throw std::runtime_error("fixture missing");std::string line;unsigned count=0;
 while(std::getline(input,line)){
  if(line.empty()||line[0]=='#')continue;unsigned secondary,status;std::uint32_t start,now,walk,run,ex,ey,ez;float range,px,py,pz;std::string events;std::istringstream row(line);
  row>>secondary>>status>>start>>now>>walk>>run>>range>>px>>py>>pz>>events>>ex>>ey>>ez;if(!row)throw std::runtime_error("fixture parse");
  MobRuntime mob;mob.status=status;mob.position={px,py,pz};mob.previous_position={1,2,3};mob.move_direction={0.5f,-0.25f,2};mob.previous_move_time=start;mob.move_range=range;
  Services s;s.secondary=secondary;s.now=now;s.put(0x5c,walk);s.put(0x64,run);auto p=s.mob_current_position(mob);
  if(std::bit_cast<std::uint32_t>(p.x)!=ex||std::bit_cast<std::uint32_t>(p.y)!=ey||std::bit_cast<std::uint32_t>(p.z)!=ez||s.clocks!=1||events!=(secondary?"T":"TS"))throw std::runtime_error("position mismatch case "+std::to_string(count));
  ++count;
 }
 if(count!=324)throw std::runtime_error("incomplete fixture");
 Services s;MobRuntime mob;s.record[0x3f]=std::byte{17};if(s.mob_size(mob)!=17)throw std::runtime_error("definition size");
 s.now=0;bool rejected=false;try{s.mob_current_position(mob);}catch(const std::logic_error&){rejected=true;}if(!rejected)throw std::runtime_error("zero divisor accepted");
 mob.state_stack=std::vector<std::int32_t>{3};mob.previous_position={1,2,3};mob.move_direction={1,1,1};mob.move_range=4;s.put(0x5c,1);s.put(0x64,10);s.now=10;
 auto p=s.mob_current_position(mob);if(p.x!=5||p.y!=6||p.z!=7)throw std::runtime_error("stacked mob status");
 std::cout<<count<<" original mob position cases passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
