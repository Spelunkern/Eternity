#include "eternity/game/vertical_combat_retarget.hpp"
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
struct Services:VerticalCombatRetargetServices {
 using VerticalCombatRetargetServices::VerticalCombatRetargetServices;
 std::array<std::byte,268> definition{};std::uint32_t exid=0;Clock* clock=nullptr;
 std::span<const std::byte> mob_definition_record(const MobRuntime&)override{return definition;}
 void log_damage(MobRuntime&,std::uint32_t)override{unexpected();}
 std::uint32_t maximum_aggro_ex(MobRuntime& mob)override{clock->events+='X';mob.retarget_time=700;return exid;}
};
int main(int argc,char**argv){try{
 if(argc!=2)throw std::runtime_error("fixture required");std::ifstream input(argv[1]);if(!input)throw std::runtime_error("fixture missing");
 auto items=ItemDefinitions::load(item_definitions());EnchantValues enchant;MobAttackTemplates templates;ActorEquipmentRules rules{items,enchant,templates};Scene scene;Actor actor(42,{}, {},{},scene,rules),target(43,{}, {},{},scene,rules);
 ZoneWorld zone_world;ZoneMobs mobs(MapGeometry(1),zone_world);ZoneUsers users(1,1,0);MobDeathExperience experience;World world;PartyMembershipUser member;member.id=42;Ai ai;Clock clock;Services services(actor,member,experience,world,ai,clock,mobs,users);services.clock=&clock;services.definition[0x78]=std::byte{1};
 unsigned count=0;std::string line;while(std::getline(input,line)){
  if(line.empty()||line[0]=='#')continue;unsigned stacked,fixed,scenario,found,kind,present,exid,estatus,etarget_kind,etarget_id,enext_move,enext_target,ecount,einterval;std::uint32_t next;float distance;std::string events;std::istringstream row(line);
  row>>stacked>>next>>fixed>>scenario>>distance>>found>>kind>>present>>exid>>events>>estatus>>etarget_kind>>etarget_id>>enext_move>>enext_target>>ecount>>einterval;if(!row)throw std::runtime_error("bad fixture");
  Pool allocator;MobDamagePool pool(allocator);MobRuntime mob;mob.damage=std::make_unique<MobDamageLedger>(pool);if(present)mob.damage->add(kind,43,10,5,0);mob.zone=&mobs;mob.status=scenario==9?2:0;mob.target_kind=7;mob.target_id=44;mob.next_move=77;mob.next_target=next;mob.vision_fixed_user=fixed;mob.lua_attacked_count=9;mob.retarget_time=300;
  target.appearance().identity.country=scenario==1?1:2;target.appearance().attackable=scenario!=2;target.appearance().visible=scenario!=3;target.attack_state().mode=scenario==5?1:0;target.status().no_mob_targeting=scenario==6?1:0;
  users.detach(target.id());if(found)users.attach(target,0,0,{distance,0,0});clock.events.clear();services.exid=exid;
  services.retarget(mob,stacked);std::string native_events;for(char c:events)if(c=='T'||c=='X')native_events+=c;
  if(mob.status!=static_cast<int>(estatus)||static_cast<unsigned>(mob.target_kind)!=etarget_kind||mob.target_id!=etarget_id||mob.next_move!=enext_move||mob.next_target!=enext_target||mob.lua_attacked_count!=ecount||mob.retarget_time!=einterval||clock.events!=native_events)throw std::runtime_error("retarget mismatch case "+std::to_string(count));++count;
 }
 if(count!=480)throw std::runtime_error("incomplete fixture");std::cout<<count<<" original retarget cases passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
