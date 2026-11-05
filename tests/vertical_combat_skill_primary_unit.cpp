#include "eternity/game/vertical_combat_skill_primary.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace eternity;using namespace eternity::game;
namespace {
void event(std::string& out,const std::string& e){if(!out.empty())out+=',';out+=e;}
struct Scene:ActorScene {
 std::string* events=nullptr;
 void send_self(const net::Packet&) noexcept override{} void send_cell_view(const net::Packet&) override{} void release_effect_id(std::uint32_t) noexcept override{}
 std::optional<bool> party_kind() const noexcept override{event(*events,"H");return {};} void send_nearby(const net::Packet&) override{} void send_party(const net::Packet&) noexcept override{} void reset_damage() override{throw std::runtime_error("unexpected reset");}
 std::optional<std::uint32_t> allocate_effect_id() noexcept override{return {};} std::uint32_t tick_ms() const noexcept override{return 0;}
 void stop_target_mob(std::uint32_t) override{} void taunt(std::uint16_t,float) override{} ZoneUsers* user_zone() override{return nullptr;} UserShapeSocial user_shape_social() override{return {};}
 void finish_periodic_damage(EffectStatus&,const SkillDefinition&,const EffectSource&,std::uint32_t) override{}
};
std::string hex(const net::Packet& p){constexpr char d[]="0123456789abcdef";std::string s;for(auto b:p){s+=d[b>>4];s+=d[b&15];}return s;}
struct World:VerticalCombatSkillPrimaryWorld {
 std::string events;std::array<std::int32_t,3> initial{},changed{};bool killed=false,zone=false,first=false,drain=false;
 bool apply_primary(Actor&,MobRuntime&,const UserSkillDefinition&,std::array<std::int32_t,3>& d)override{
  if(d!=initial)throw std::runtime_error("input damage");d=changed;event(events,"D");if(killed){event(events,"S");event(events,"X");}return killed;
 }
 void send_primary_view(Actor&,const net::Packet& p)override{if(zone)event(events,"P"+hex(p));}
 void after_primary(Actor&,MobRuntime&,const UserSkillDefinition&,bool f,bool second,bool k,bool d,std::array<std::int32_t,3>& damage)override{
  if(f!=first||!second||k!=killed||d!=drain||damage!=changed)throw std::runtime_error("continuation arguments");event(events,"B");
 }
};
}
int main(int argc,char**argv){try{
 if(argc!=2)throw std::runtime_error("fixture required");std::ifstream input(argv[1]);if(!input)throw std::runtime_error("missing fixture");
 ItemDefinitions items;EnchantValues enchant;MobAttackTemplates templates;ActorEquipmentRules rules{items,enchant,templates};std::string line;unsigned count=0;
 while(std::getline(input,line)){
  if(line.empty()||line[0]=='#')continue;unsigned first,detail,critical,killed,zone,hp,sp,mp;std::string expected;World world;std::istringstream row(line);
  row>>first>>detail>>critical>>killed>>zone;for(auto& v:world.initial)row>>v;for(auto& v:world.changed)row>>v;row>>expected>>hp>>sp>>mp;if(!row)throw std::runtime_error("bad fixture");
  Scene scene;scene.events=&world.events;EffectStatus state;state.skills.equipment.base.maximum={1000,1000,1000};Actor actor(42,state,PlayerResources{10,30,20},{},scene,rules);MobRuntime mob;mob.id=77;
  SkillDefinition effect;effect.id=11;effect.level=3;effect.detail=detail;UserSkillDefinition info{effect,"test"};world.first=first;world.drain=first&&detail==27;world.killed=killed;world.zone=zone;
  auto damage=world.initial;vertical_combat_skill_primary(actor,mob,info,first,true,critical,damage,world);
  if(world.events!=expected||actor.resources().hp!=hp||actor.resources().sp!=sp||actor.resources().mp!=mp)throw std::runtime_error("case "+std::to_string(count)+" got "+world.events+" expected "+expected);
  ++count;
 }
 if(count!=128)throw std::runtime_error("incomplete fixture");
 std::cout<<count<<" original primary skill hit comparisons passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
