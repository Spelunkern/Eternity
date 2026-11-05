#include "eternity/game/vertical_combat_skill_hit_prefix.hpp"
#include <bit>
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
struct World:VerticalCombatSkillHitSelectServices {
 bool success=false,zone=false;unsigned after=0;bool expected_first=false;std::string trace;
 void event(const std::string& s){if(!trace.empty())trace+=',';trace+=s;}
 bool skill_success(Actor&,MobRuntime&,const UserSkillDefinition&) override{event("C");return success;}
 void remove_sleep(MobRuntime&) override{event("S");}
 void send_skill_view(Actor&,const net::Packet& p) override{if(zone)event("P"+hex(p));}
 void set_target(MobRuntime& m,Actor&) override{event("T");m.battle_type=after;}
 void continue_hit(Actor&,MobRuntime&,const UserSkillDefinition&,bool first,bool second) override{if(first!=expected_first||!second)throw std::runtime_error("continuation flags");event("D");}
 void announce_multi(Actor&,MobRuntime&,const UserSkillDefinition&) override{throw std::runtime_error("unexpected announce");}
 bool self_damage(Actor&,std::int32_t) override{throw std::runtime_error("unexpected self damage");}
 void stop_mob_attack(MobRuntime&) override{throw std::runtime_error("unexpected stop");}
 void self_death(Actor&) override{throw std::runtime_error("unexpected death");}
 void remaining_mob_effect(UserSkillAction,Actor&,MobRuntime*,const UserSkillDefinition&) override{throw std::runtime_error("unexpected action");}
 void user_effect(UserSkillAction,Actor&,Actor*,const UserSkillDefinition&) override{throw std::runtime_error("unexpected user effect");}
 void spend(Actor&,std::uint16_t,std::uint16_t) override{throw std::runtime_error("unexpected spend");}
 std::uint32_t tick_ms() override{throw std::runtime_error("unexpected tick");}
};
}
int main(int argc,char** argv){try{
 if(argc!=2)throw std::runtime_error("fixture required");std::ifstream input(argv[1]);if(!input)throw std::runtime_error("fixture missing");
 ItemDefinitions items;EnchantValues enchant;MobAttackTemplates templates;ActorEquipmentRules rules{items,enchant,templates};std::string line;unsigned count=0;
 while(std::getline(input,line)){
  if(line.empty()||line[0]=='#')continue;unsigned first,success,detail,skill,zone,battle,after;std::uint32_t speed,result;std::string expected,flags;
  std::istringstream row(line);row>>first>>success>>detail>>skill>>zone>>battle>>after>>speed>>expected>>flags>>result;if(!row)throw std::runtime_error("bad fixture");
  Scene scene;EffectStatus state;Actor actor(42,state,{}, {},scene,rules);actor.movement_check().maximum_speed=std::bit_cast<float>(speed);
  MobRuntime mob;mob.id=77;mob.battle_type=battle;SkillDefinition effect;effect.id=skill;effect.level=3;effect.detail=detail;UserSkillDefinition info{effect,"test"};info.attack_kind=1;info.attack_range=7;
  World world;world.success=success;world.zone=zone;world.after=after;world.expected_first=first;
  world.hit(actor,mob,info,first,true);
  const std::string actual_flags=mob.attacked&&mob.in_battle&&mob.attack_at_moment?"010101":"000000";
  if(world.trace!=expected||flags!=actual_flags||std::bit_cast<std::uint32_t>(actor.movement_check().maximum_speed)!=result)
   throw std::runtime_error("case "+std::to_string(count)+" got "+world.trace+" expected "+expected);
  ++count;
 }
 if(count!=256)throw std::runtime_error("incomplete fixture");
 Scene scene;Actor actor(42,{}, {},{},scene,rules);MobRuntime mob;SkillDefinition effect;effect.effect=3;UserSkillDefinition info{effect,"debuff"};
 World world;world.success=true;world.expected_first=true;world.mob_effect(UserSkillAction::debuff_select,actor,&mob,info);
 if(world.trace!="C,S,D")throw std::runtime_error("selected debuff did not reach hit prefix");
 bool rejected=false;try{world.mob_effect(UserSkillAction::debuff_select,actor,nullptr,info);}catch(const std::invalid_argument&){rejected=true;}
 if(!rejected)throw std::runtime_error("selected debuff accepted missing mob");
 std::cout<<count<<" original selected skill hit prefix comparisons and selected debuff routing passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
