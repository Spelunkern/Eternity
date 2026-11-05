#include "eternity/game/vertical_combat_skill_select.hpp"
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
struct World:VerticalCombatSkillSelectServices {
 UserSkillDefinition& info;unsigned announced,changed,dead,status,killed,hits=0;std::string trace;
 explicit World(UserSkillDefinition& i):info(i){}
 void event(const std::string& s){if(!trace.empty())trace+=',';trace+=s;}
 void announce_multi(Actor&,MobRuntime&,const UserSkillDefinition&) override{event("A");info.multi_attack=announced;}
 void hit(Actor& actor,MobRuntime& mob,const UserSkillDefinition&,bool first,bool second) override {
  event("H"+std::to_string(first)+std::to_string(second));
  if(++hits==1){info.multi_attack=changed;actor.attack_state().mode=dead;mob.status=status;
   actor.heal({std::bit_cast<std::int32_t>(0x80000011u-actor.resources().hp),0,0});}
 }
 bool self_damage(Actor&,std::int32_t hp) override{event("D"+std::to_string(std::bit_cast<std::uint32_t>(hp)));return killed;}
 void stop_mob_attack(MobRuntime&) override{event("S");}
 void self_death(Actor&) override{event("F");}
 void other_mob_effect(UserSkillAction,Actor&,MobRuntime*,const UserSkillDefinition&) override{event("O");}
 void user_effect(UserSkillAction,Actor&,Actor*,const UserSkillDefinition&) override{throw std::runtime_error("unexpected user effect");}
 void spend(Actor&,std::uint16_t,std::uint16_t) override{throw std::runtime_error("unexpected spend");}
 std::uint32_t tick_ms() override{throw std::runtime_error("unexpected tick");}
};
}
int main(int argc,char** argv){try{
 if(argc!=2)throw std::runtime_error("fixture required");std::ifstream input(argv[1]);if(!input)throw std::runtime_error("fixture missing");
 ItemDefinitions items;EnchantValues enchant;MobAttackTemplates templates;ActorEquipmentRules rules{items,enchant,templates};std::string line;unsigned count=0;
 while(std::getline(input,line)){
  if(line.empty()||line[0]=='#')continue;
  unsigned detail,initial,announced,changed,dead,status,killed;std::string expected;std::istringstream row(line);
  row>>detail>>initial>>announced>>changed>>dead>>status>>killed>>expected;if(!row)throw std::runtime_error("bad fixture");
  Scene scene;EffectStatus state;state.skills.equipment.base.maximum={0x7fffffff,0x7fffffff,0x7fffffff};
  Actor actor(42,state,PlayerResources{13,100,100},{},scene,rules);MobRuntime mob;mob.id=77;mob.status=1;
  SkillDefinition effect;effect.detail=detail;UserSkillDefinition info{effect,"test"};info.multi_attack=initial;
  World world(info);world.announced=announced;world.changed=changed;world.dead=dead;world.status=status;world.killed=killed;
  world.mob_effect(UserSkillAction::attack_select,actor,&mob,info);
  // Status is read by real mob_status, rather than an intercepted call.
  std::string filtered;std::istringstream tokens(expected);std::string token;
  while(std::getline(tokens,token,',')){if(token=="T"||token=="-")continue;if(!filtered.empty())filtered+=',';filtered+=token;}
  if(world.trace!=filtered)throw std::runtime_error("case "+std::to_string(count)+" got "+world.trace+" expected "+filtered);
  ++count;
 }
 if(count!=1280)throw std::runtime_error("incomplete fixture");
 // A stacked FSM overrides the raw live status when deciding whether to continue.
 Scene scene;EffectStatus state;Actor actor(42,state,{}, {},scene,rules);MobRuntime mob;mob.status=1;mob.state_stack=std::vector<std::int32_t>{2};
 SkillDefinition effect;effect.detail=22;UserSkillDefinition info{effect,"test"};info.multi_attack=3;
 World world(info);world.announced=3;world.changed=3;world.dead=0;world.status=1;world.killed=0;
 vertical_combat_skill_select(actor,mob,info,world);if(world.trace!="A,H01")throw std::runtime_error("stacked status guard");
 world.trace.clear();world.mob_effect(UserSkillAction::debuff_around,actor,nullptr,info);if(world.trace!="O")throw std::runtime_error("other action forwarding");
 bool rejected=false;try{world.mob_effect(UserSkillAction::attack_select,actor,nullptr,info);}catch(const std::invalid_argument&){rejected=true;}
 if(!rejected)throw std::runtime_error("missing selected target");
 std::cout<<count<<" original selected mob skill comparisons passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
