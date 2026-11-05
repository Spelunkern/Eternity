#include "eternity/game/vertical_combat_definition.hpp"
#include "eternity/game/item_pool.hpp"
#include "equipment_fixture.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
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
}
[[noreturn]] void unexpected(){throw std::runtime_error("unexpected provider");}
struct Pool:MobDamagePoolServices {std::unique_ptr<MobDamageRecord> construct()override{return std::make_unique<MobDamageRecord>();}};
struct World:MobDeathWorld {
 MobDeathUser user{42,1,0};std::string events;std::int32_t experience=0;
 void send_death(MobRuntime&,const net::Packet&,std::uint8_t)override{unexpected();}
 MobDeathUser* find_user(MobRuntime& mob,std::uint32_t id)override{if(mob.damage->records().size()!=1||mob.damage->records()[0]->aggro!=90)unexpected();events+='F';return id==user.id?&user:nullptr;}
 MobDeathParty* party(MobDeathUser&)override{return nullptr;}
 void lock_party(MobDeathParty&)override{unexpected();}void unlock_party(MobDeathParty&)noexcept override{}
 bool in_reward_range(MobDeathUser&,MobDeathUser&,std::int32_t)override{unexpected();}
 void add_experience(MobDeathUser& u,std::uint32_t source,std::int32_t value)override{if(&u!=&user||source!=42)unexpected();events+='E';experience=value;}
 void drop_items(MobRuntime&,MobDeathUser* u,MobDeathParty* p)override{if(u!=&user||p)unexpected();events+='D';}
 void quest_kill(MobDeathUser& u,MobRuntime&)override{if(&u!=&user)unexpected();events+='Q';}
 std::optional<std::int32_t> obelisk_country(MobRuntime&,std::int32_t)override{unexpected();}
 void destroy_obelisk(MobRuntime&,std::int32_t,std::int32_t)override{unexpected();}void destroy_linked_group(MobRuntime&,std::int32_t)override{unexpected();}
 void remove_obelisk_member(MobRuntime&,void*)override{unexpected();}void remove_linked_member(MobRuntime&,void*)override{unexpected();}
 void log_death(MobRuntime& mob,MobDeathUser* credited,bool fair)override{if(credited!=&user||!fair||mob.damage->records().empty()||!mob.state_stack||mob.state_stack->back()!=2)unexpected();events+='L';}
 void clear_applied_skills(MobRuntime& mob)override{if(!mob.damage->records().empty())unexpected();events+='C';}
};
struct Ai:MobAiStateServices {
 unsigned calls=0;std::uint32_t tick()override{return 333;}
 void lua_call(MobRuntime& mob,std::string_view name,std::uint32_t at,std::optional<std::uint32_t> argument)override{
  if(name!="OnDeath"||at!=333||argument!=12||mob.hp!=0||mob.status!=2||!mob.state_stack||mob.state_stack->back()!=1||mob.function_type!=4)unexpected();++calls;
 }
 void update(MobRuntime&,MobAiState)override{unexpected();}void error(MobRuntime&,MobAiError,std::int32_t,std::int32_t)override{unexpected();}
};
struct Clock:VerticalCombatClock {std::string events;std::uint32_t tick_ms()override{events+='T';return 1000;}};
struct ZoneWorld:ZoneMobServices {
 std::uint32_t tick_ms()override{unexpected();}void send_cell_view(CellCoordinates,const net::Packet&)override{unexpected();}
 std::unique_ptr<MobBroadcast> allocate_broadcast(const net::Packet&,std::optional<std::uint8_t>)override{unexpected();}void lua_init(MobRuntime&)override{unexpected();}
};
struct Services:VerticalCombatDefinitionServices {
 using VerticalCombatDefinitionServices::VerticalCombatDefinitionServices;
 void log_damage(MobRuntime&,std::uint32_t)override{unexpected();}
};
int main(int argc,char**argv){try{
 if(argc!=2)throw std::runtime_error("fixture required");std::ifstream input(argv[1]);if(!input)throw std::runtime_error("fixture missing");
 auto items=ItemDefinitions::load(item_definitions());EnchantValues enchant;MobAttackTemplates templates;ActorEquipmentRules rules{items,enchant,templates};Scene scene;Actor actor(42,{}, {},{},scene,rules),first(43,{}, {},{},scene,rules),second(44,{}, {},{},scene,rules);
 ZoneWorld zone_world;ZoneMobs mobs(MapGeometry(1),zone_world);ZoneUsers users(1,1,0);users.attach(first,0,0,{44,0,0});first.appearance().identity.country=2;first.appearance().attackable=true;first.appearance().visible=true;
 storage::ResultSet rows;rows.columns={"MobID","Level","AI","HP","SP","MP","Dex","Wis","Luc","Attrib","Defense","Magic","Attacktime3","Attackspecial3","Exp"};rows.rows.push_back({"1","1","0","1","0","0","0","0","0","0","0","0","0","0","0"});rows.rows.push_back({"2","1","0","1","0","0","0","0","0","0","0","0","0","0","0"});auto definitions=MobRuntimeTemplates::load(rows);
 auto catalog=MobTargetingBindingCatalog::load(1,0,{},{});auto raw=catalog.mutable_mob_record(1);raw[0]=std::byte{1};raw[0x78]=std::byte{1};catalog.publish_mob_record(1);
 MobDeathExperience experience;World world;PartyMembershipUser member;member.id=42;Ai ai;Clock clock;Services services(actor,member,experience,world,ai,clock,mobs,users,definitions,catalog);
 unsigned count=0;std::string line;while(std::getline(input,line)){
  if(line.empty()||line[0]=='#')continue;unsigned kind1,kind2,scenario,expected;int aggro1,aggro2;float distance;std::istringstream row(line);row>>kind1>>kind2>>aggro1>>aggro2>>scenario>>distance>>expected;if(!row)throw std::runtime_error("bad fixture");
  Pool allocator;MobDamagePool pool(allocator);MobRuntime mob;mob.damage=std::make_unique<MobDamageLedger>(pool);mob.damage->add(kind1,43,0,aggro1,0);mob.damage->add(kind2,44,0,aggro2,0);mob.zone=&mobs;mob.definition=definitions.find(1);mob.target_kind=7;mob.target_id=999;mob.retarget_time=300;mob.next_target=0;mob.state_stack=std::vector<std::int32_t>{1};
  second.appearance().identity.country=scenario==1?1:2;second.attack_state().mode=scenario==2?1:0;second.status().no_mob_targeting=scenario==3?1:0;second.appearance().visible=scenario!=4;second.appearance().attackable=scenario!=6;
  users.detach(second.id());if(scenario!=5)users.attach(second,0,0,{distance,0,0});
  if(services.maximum_aggro_ex(mob)!=expected)throw std::runtime_error("aggro selection mismatch case "+std::to_string(count));clock.events.clear();services.retarget(mob,true);
  if(mob.target_id!=(expected?expected:999)||mob.target_kind!=(expected?1:7)||mob.next_target!=1300||mob.lua_attacked_count!=1||clock.events!="T")throw std::runtime_error("stacked retarget composition case "+std::to_string(count));++count;
 }
 if(count!=672)throw std::runtime_error("incomplete fixture");
 MobRuntime checked;checked.definition=definitions.find(1);if(services.mob_definition_record(checked).data()!=catalog.mob_record(1).data())throw std::runtime_error("definition copy");raw[0x6c]=std::byte{1};if(!services.long_corpse_delay(checked))throw std::runtime_error("live definition reload");raw[0x6c]=std::byte{0};raw[0]=std::byte{3};catalog.publish_mob_record(1);bool rejected=false;try{services.mob_definition_record(checked);}catch(const std::logic_error&){rejected=true;}if(!rejected)throw std::runtime_error("mismatched catalog ID accepted");raw[0]=std::byte{1};catalog.publish_mob_record(1);
 MobRuntimeTemplate foreign;foreign.id=1;checked.definition=&foreign;rejected=false;try{services.mob_definition_record(checked);}catch(const std::invalid_argument&){rejected=true;}if(!rejected)throw std::runtime_error("foreign template accepted");checked.definition=definitions.find(2);rejected=false;try{services.mob_definition_record(checked);}catch(const std::logic_error&){rejected=true;}if(!rejected)throw std::runtime_error("missing catalog record accepted");
 std::cout<<count<<" original stacked aggro selection cases passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
