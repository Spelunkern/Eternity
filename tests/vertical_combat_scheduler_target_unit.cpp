#include "eternity/game/vertical_combat_scheduler_target.hpp"
#include "equipment_fixture.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace eternity;using namespace eternity::game;
namespace {
struct Scene:ActorScene {
 void send_self(const net::Packet&)noexcept override{}void send_cell_view(const net::Packet&)override{}void release_effect_id(std::uint32_t)noexcept override{}
 std::optional<bool> party_kind()const noexcept override{return {};}void send_nearby(const net::Packet&)override{}void send_party(const net::Packet&)noexcept override{}void reset_damage()override{}
 std::optional<std::uint32_t> allocate_effect_id()noexcept override{return {};}std::uint32_t tick_ms()const noexcept override{return 0;}
 void stop_target_mob(std::uint32_t)override{}void taunt(std::uint16_t,float)override{}ZoneUsers* user_zone()override{return nullptr;}UserShapeSocial user_shape_social()override{return {};}
 void finish_periodic_damage(EffectStatus&,const SkillDefinition&,const EffectSource&,std::uint32_t)override{}
};
struct ZoneWorld:ZoneMobServices {
 std::uint32_t tick_ms()override{throw std::logic_error("unexpected clock");}void send_cell_view(CellCoordinates,const net::Packet&)override{}
 std::unique_ptr<MobBroadcast> allocate_broadcast(const net::Packet&,std::optional<std::uint8_t>)override{throw std::logic_error("unexpected broadcast");}void lua_init(MobRuntime&)override{}
};
}
int main(int argc,char**argv){try{
 if(argc!=2)throw std::runtime_error("fixture required");std::ifstream input(argv[1]);if(!input)throw std::runtime_error("fixture missing");
 auto items=ItemDefinitions::load(item_definitions());EnchantValues enchant;MobAttackTemplates attack_templates;ActorEquipmentRules rules{items,enchant,attack_templates};Scene scene;Actor actor(42,{}, {},{},scene,rules);
 storage::ResultSet rows;rows.columns={"MobID","Level","AI","HP","SP","MP","Dex","Wis","Luc","Attrib","Defense","Magic","Attacktime3","Attackspecial3","Exp"};rows.rows.push_back({"1","1","0","1","0","0","0","0","0","0","0","0","0","0","0"});auto definitions=MobRuntimeTemplates::load(rows);
 auto catalog=MobTargetingBindingCatalog::load(1,0,{},{});auto raw=catalog.mutable_mob_record(1);raw[0]=std::byte{1};catalog.publish_mob_record(1);
 ZoneWorld world;ZoneMobs mobs(MapGeometry(1),world);ZoneUsers users(1,1,0);std::string line;unsigned count=0;
 while(std::getline(input,line)){
  if(line.empty()||line[0]=='#')continue;unsigned zoned,found,status,user_country,mob_country,lua,spawn,accepted,both;std::string events;std::istringstream row(line);row>>zoned>>found>>status>>user_country>>mob_country>>lua>>spawn>>accepted>>both>>events;if(!row)throw std::runtime_error("bad fixture");
  users.detach(actor.id());if(zoned)users.attach(actor,0,0,{});actor.appearance().identity.country=user_country;raw[0x78]=std::byte(mob_country);
  MobRespawn respawn;respawn.both=spawn==2;MobRuntime mob;mob.id=77;mob.status=status;mob.definition=definitions.find(1);mob.lua_ai=lua;mob.respawn=spawn?&respawn:nullptr;
  if(found&&!mobs.enter(mob))throw std::runtime_error("mob entry");const bool result=vertical_combat_enable_mob(actor,77,users,mobs,definitions,catalog)!=nullptr;const bool linked=vertical_combat_is_both_mob(mob);if(found)mobs.leave(mob);
  if(result!=bool(accepted)||linked!=bool(both)||events!=(zoned?"F":"-"))throw std::runtime_error("original target mismatch case "+std::to_string(count));++count;
 }
 if(count!=648)throw std::runtime_error("incomplete fixture");
 users.detach(actor.id());users.attach(actor,0,0,{});actor.appearance().identity.country=0;raw[0x78]=std::byte{1};MobRuntime mob;mob.id=77;mob.definition=definitions.find(1);mob.state_stack=std::vector<std::int32_t>{2};if(!mobs.enter(mob))throw std::runtime_error("stacked mob entry");
 if(vertical_combat_enable_mob(actor,77,users,mobs,definitions,catalog))throw std::runtime_error("dead stacked mob accepted");mob.state_stack->clear();if(vertical_combat_enable_mob(actor,77,users,mobs,definitions,catalog)!=&mob)throw std::runtime_error("empty stacked mob rejected");mobs.leave(mob);
 std::cout<<count<<" original scheduler target cases passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
