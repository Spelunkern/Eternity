#include "eternity/game/vertical_combat_log_context.hpp"
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
struct Transport:MobLogTransport {
 std::vector<net::Packet> packets;std::array<std::int32_t,3>* damage=nullptr;
 void send_game_log(const net::Packet& packet)override{packets.push_back(packet);if(damage)(*damage)[0]=40;}
};
std::uint32_t word(const net::Packet& p,unsigned at){return p[at]|(std::uint32_t(p[at+1])<<8)|(std::uint32_t(p[at+2])<<16)|(std::uint32_t(p[at+3])<<24);}
int main(){try{
 auto items=ItemDefinitions::load(item_definitions());EnchantValues enchant;MobAttackTemplates templates;ActorEquipmentRules rules{items,enchant,templates};Scene scene;Actor actor(42,{}, {},{},scene,rules);
 ZoneWorld zone_world;ConfiguredMap map;RuntimeZone zone;map.settings.id=5;zone.map=&map;zone.mobs=std::make_unique<ZoneMobs>(MapGeometry(1),zone_world);auto& mobs=*zone.mobs;ZoneUsers users(1,1,0);
 storage::ResultSet rows;rows.columns={"MobID","Level","AI","HP","SP","MP","Dex","Wis","Luc","Attrib","Defense","Magic","Attacktime3","Attackspecial3","Exp"};rows.rows.push_back({"1","1","0","1","0","0","0","0","0","0","0","0","0","0","0"});auto definitions=MobRuntimeTemplates::load(rows);
 auto catalog=MobTargetingBindingCatalog::load(1,0,{},{});auto raw=catalog.mutable_mob_record(1);raw[0]=std::byte{1};raw[2]=std::byte{'M'};raw[3]=std::byte{'o'};raw[4]=std::byte{'b'};catalog.publish_mob_record(1);
 MobDeathExperience experience;World world;PartyMembershipUser member;member.id=42;Ai ai;VerticalCombatFsmClock clock(ai);VerticalCombatRetainedLogContext context(zone,definitions,catalog);Transport transport;
 VerticalCombatLogServices services(actor,member,experience,world,ai,clock,mobs,users,definitions,catalog,context,transport);if(services.tick_ms()!=333)throw std::runtime_error("FSM clock delegation");
 Pool allocator;MobDamagePool pool(allocator);MobRuntime mob;mob.damage=std::make_unique<MobDamageLedger>(pool);mob.definition=definitions.find(1);mob.id=77;mob.zone=&mobs;mob.hp=100;mob.mp=100;mob.lua_ai=1;mob.previous_attack_user[0]='A';mob.previous_attack_skill[0]='S';
 raw[2]=std::byte{'N'};if(context.context(mob).definition_name[0]!='N')throw std::runtime_error("live definition name");raw[2]=std::byte{'M'};
 MobRuntimeTemplate foreign;foreign.id=1;mob.definition=&foreign;bool rejected=false;try{context.context(mob);}catch(const std::invalid_argument&){rejected=true;}if(!rejected)throw std::runtime_error("foreign template accepted");mob.definition=definitions.find(1);
 map.settings.id=-1;rejected=false;try{context.context(mob);}catch(const std::invalid_argument&){rejected=true;}if(!rejected)throw std::runtime_error("invalid map accepted");map.settings.id=5;
 ZoneMobs foreign_zone(MapGeometry(1),zone_world);mob.zone=&foreign_zone;rejected=false;try{context.context(mob);}catch(const std::invalid_argument&){rejected=true;}if(!rejected)throw std::runtime_error("foreign zone accepted");mob.zone=&mobs;
 raw[0]=std::byte{3};catalog.publish_mob_record(1);rejected=false;try{context.context(mob);}catch(const std::logic_error&){rejected=true;}if(!rejected)throw std::runtime_error("mismatched mob record accepted");raw[0]=std::byte{1};catalog.publish_mob_record(1);
 std::array<std::int32_t,3> damage{25,1,0};transport.damage=&damage;
 if(services.damage(actor,mob,damage,MobCombatCredit{1,42,0,0,0})||mob.hp!=75||mob.mp!=60||mob.damage->records().size()!=1||mob.damage->records()[0]->damage!=40||transport.packets.size()!=1)throw std::runtime_error("damage log reentry");
 const auto& packet=transport.packets[0];if(packet.size()!=123||packet[0]!=4||packet[1]!=10||packet[2]!=5||packet[16]!=2||packet[17]!='M'||packet[49]!='A'||packet[81]!='S'||word(packet,113)!=25||word(packet,117)!=75||packet[121]!=1)throw std::runtime_error("retained damage log packet");
 mob.zone=nullptr;zone.map=nullptr;services.log_damage(mob,99);if(transport.packets.size()!=1)throw std::runtime_error("absent zone log");
 std::cout<<"retained damage log and FSM clock composition passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
