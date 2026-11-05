#include "eternity/game/vertical_combat_skill_tail.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace eternity;using namespace eternity::game;
namespace {
struct Scene:ActorScene {
 void send_self(const net::Packet&) noexcept override{} void send_cell_view(const net::Packet&) override{} void release_effect_id(std::uint32_t) noexcept override{}
 std::optional<bool> party_kind() const noexcept override{return {};} void send_nearby(const net::Packet&) override{} void send_party(const net::Packet&) noexcept override{} void reset_damage() override{}
 std::optional<std::uint32_t> allocate_effect_id() noexcept override{return {};} std::uint32_t tick_ms() const noexcept override{return 0;}
 void stop_target_mob(std::uint32_t) override{} void taunt(std::uint16_t,float) override{} ZoneUsers* user_zone() override{return nullptr;} UserShapeSocial user_shape_social() override{return {};}
 void finish_periodic_damage(EffectStatus&,const SkillDefinition&,const EffectSource&,std::uint32_t) override{}
};
std::string hex(const net::Packet& p){constexpr char d[]="0123456789abcdef";std::string s;for(auto b:p){s+=d[b>>4];s+=d[b&15];}return s;}
struct World:VerticalCombatSkillTailWorld {
 SkillDefinition& effect;bool extra=false,zone=false,secondary_killed=false;unsigned target_effect=0;std::string events="B";
 explicit World(SkillDefinition& e):effect(e){}
 void event(const std::string& e){events+=';';events+=e;}
 std::array<std::uint16_t,3> secondary_words(const UserSkillDefinition&)override{return extra?std::array<std::uint16_t,3>{11,22,33}:std::array<std::uint16_t,3>{};}
 bool apply_secondary(Actor&,MobRuntime&,const UserSkillDefinition&,std::array<std::int32_t,3>& primary,std::array<std::int32_t,3>& damage)override{
  if(primary!=std::array<std::int32_t,3>{100,200,300})throw std::runtime_error("primary credit");
  event("D"+std::to_string(damage[0])+","+std::to_string(damage[1])+","+std::to_string(damage[2]));damage={99,88,77};
  if(secondary_killed){event("S");event("X");}return secondary_killed;
 }
 void send_secondary(Actor&,bool first,const net::Packet& packet)override{if(first)event("C"+hex(packet));else if(zone)event("P"+hex(packet));}
 void set_target(MobRuntime&,Actor&)override{event("T");effect.effect=target_effect;}
 void add_mob_skill(MobRuntime&,Actor&,const UserSkillDefinition&)override{event("E");}
 void attack_item_option(Actor&,MobRuntime&)override{event("I");}
};
}
int main(int argc,char**argv){try{
 if(argc!=2)throw std::runtime_error("fixture required");std::ifstream input(argv[1]);if(!input)throw std::runtime_error("missing fixture");
 ItemDefinitions items;EnchantValues enchant;MobAttackTemplates templates;ActorEquipmentRules rules{items,enchant,templates};std::string line;unsigned count=0;
 while(std::getline(input,line)){
  if(line.empty()||line[0]=='#')continue;unsigned first,drain,primary_killed,secondary_killed,extra,zone,second,target_effect;std::string expected,flags;std::istringstream row(line);
  row>>first>>drain>>primary_killed>>secondary_killed>>extra>>zone>>second>>target_effect>>expected>>flags;if(!row)throw std::runtime_error("bad fixture");
  Scene scene;Actor actor(42,{}, {},{},scene,rules);MobRuntime mob;mob.id=77;SkillDefinition effect;effect.id=11;effect.level=3;effect.detail=drain?27:21;effect.effect=1;UserSkillDefinition info{effect,"test"};
  World world(effect);world.extra=extra;world.zone=zone;world.secondary_killed=secondary_killed;world.target_effect=target_effect;std::array<std::int32_t,3> primary{100,200,300};
  vertical_combat_skill_tail(actor,mob,info,first,second,primary_killed,first&&drain,primary,world);
  const std::string actual=mob.attacked&&mob.in_battle&&mob.attack_at_moment?"010101":"000000";
  if(world.events!=expected||flags!=actual)throw std::runtime_error("case "+std::to_string(count)+" got "+world.events+" expected "+expected);++count;
 }
 if(count!=256)throw std::runtime_error("incomplete fixture");
 std::cout<<count<<" original secondary skill hit and tail comparisons passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
