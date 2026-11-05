#include "eternity/game/vertical_combat_item_option.hpp"
#include "eternity/game/ground_items.hpp"
#include "eternity/game/item_pool.hpp"
#include "equipment_fixture.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace eternity;using namespace eternity::game;
namespace {
[[noreturn]] void unused(){throw std::runtime_error("unexpected item option callback");}
struct Scene:ActorScene {
 void send_self(const net::Packet&)noexcept override{}void send_cell_view(const net::Packet&)override{}void release_effect_id(std::uint32_t)noexcept override{}
 std::optional<bool> party_kind()const noexcept override{return {};}void send_nearby(const net::Packet&)override{}void send_party(const net::Packet&)noexcept override{}void reset_damage()override{}
 std::optional<std::uint32_t> allocate_effect_id()noexcept override{return {};}std::uint32_t tick_ms()const noexcept override{return 0;}
 void stop_target_mob(std::uint32_t)override{}void taunt(std::uint16_t,float)override{}ZoneUsers* user_zone()override{return nullptr;}UserShapeSocial user_shape_social()override{return {};}
 void finish_periodic_damage(EffectStatus&,const SkillDefinition&,const EffectSource&,std::uint32_t)override{}
};
struct Select:VerticalCombatSkillSelectWorld {
 std::string& events;explicit Select(std::string& e):events(e){}
 void announce_multi(Actor&,MobRuntime&,const UserSkillDefinition&)override{unused();}
 void hit(Actor&,MobRuntime&,const UserSkillDefinition&,bool first,bool second)override{if(!first||!second)unused();events+='S';}
 bool self_damage(Actor&,std::int32_t)override{unused();}void stop_mob_attack(MobRuntime&)override{unused();}void self_death(Actor&)override{unused();}
};
struct Hit:VerticalCombatSkillHitPrefixWorld {
 std::string& events;explicit Hit(std::string& e):events(e){}
 bool skill_success(Actor&,MobRuntime&,const UserSkillDefinition&)override{return true;}
 void remove_sleep(MobRuntime&)override{}void send_skill_view(Actor&,const net::Packet&)override{unused();}void set_target(MobRuntime&,Actor&)override{unused();}
 void continue_hit(Actor&,MobRuntime&,const UserSkillDefinition&,bool first,bool second)override{if(!first||!second)unused();events+='H';}
};
struct World:VerticalCombatItemOptionWorld {
 std::uint8_t guard;std::array<std::byte,0x47> record{};const UserSkillDefinition& info;std::string events;Select select;Hit hit;
 World(unsigned g,unsigned enabled,unsigned id,const UserSkillDefinition& s):guard(static_cast<std::uint8_t>(g)),info(s),select(events),hit(events){record[0x24]=record[0x25]=static_cast<std::byte>(enabled);record[0x46]=static_cast<std::byte>(id);}
 std::uint8_t& active(Actor&)override{return guard;}std::span<const std::byte> jewel(std::uint8_t id)override{return id==1?std::span<const std::byte>(record):std::span<const std::byte>{};}
 const UserSkillDefinition* mob_skill(std::uint8_t id)override{return id==2?&info:nullptr;}
 VerticalCombatSkillSelectWorld& select_world()override{return select;}VerticalCombatSkillHitPrefixWorld& hit_world()override{return hit;}
};
}
int main(int argc,char** argv){try{
 if(argc!=2)throw std::runtime_error("fixture required");std::ifstream input(argv[1]);if(!input)throw std::runtime_error("fixture missing");
 auto items=ItemDefinitions::load(item_definitions());EnchantValues enchant;MobAttackTemplates templates;ActorEquipmentRules rules{items,enchant,templates};std::string line;unsigned count=0;
 while(std::getline(input,line)){
  if(line.empty()||line[0]=='#')continue;unsigned guard,status,mode,equipped,gem,enabled,skill_id,effect,expected_guard;std::string expected;
  std::istringstream row(line);row>>guard>>status>>mode>>equipped>>gem>>enabled>>skill_id>>effect>>expected_guard>>expected;if(!row)throw std::runtime_error("invalid fixture");
  Scene scene;GroundItemPool pool;Actor actor(42,{}, {},{},scene,rules);actor.attack_state().mode=static_cast<std::int32_t>(mode);
  if(equipped){auto* item=pool.allocate();if(!item)throw std::runtime_error("weapon allocation");item->definition=items.find(5,1);item->item.type=5;item->item.type_id=1;item->item.gems[0]=static_cast<std::uint8_t>(gem);if(!actor.claim_inventory_item(0,5,*item,pool))throw std::runtime_error("weapon claim");}
  MobRuntime mob;mob.status=static_cast<std::int32_t>(status);SkillDefinition definition;definition.id=2;definition.level=100;definition.effect=static_cast<std::uint8_t>(effect);definition.detail=21;UserSkillDefinition skill{definition,"oracle"};World world(guard,enabled,skill_id,skill);
  vertical_combat_item_option(actor,mob,world);const auto actual=world.events.empty()?"-":world.events;
  if(world.guard!=expected_guard||actual!=expected)throw std::runtime_error("item option mismatch case "+std::to_string(count)+" got "+actual+" expected "+expected);++count;
 }
 if(count!=96)throw std::runtime_error("incomplete original fixture");std::cout<<count<<" original item option comparisons passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
