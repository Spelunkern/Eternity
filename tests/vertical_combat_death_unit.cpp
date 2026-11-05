#include "eternity/game/vertical_combat_mob_state.hpp"
#include "eternity/game/item_pool.hpp"
#include "equipment_fixture.hpp"
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
struct Clock:VerticalCombatClock {unsigned calls=0;std::uint32_t tick_ms()override{++calls;return 123;}};
struct Services:VerticalCombatMobStateServices {
 using VerticalCombatMobStateServices::VerticalCombatMobStateServices;std::array<std::byte,0x70> record{};unsigned damage_logs=0;
 std::span<const std::byte> mob_definition_record(const MobRuntime&)override{return record;}
 void log_damage(MobRuntime& mob,std::uint32_t value)override{if(mob.hp!=0||mob.status!=2||value!=10)unexpected();++damage_logs;}void retarget(MobRuntime&,bool)override{unexpected();}
};
int main(){try{
 auto items=ItemDefinitions::load(item_definitions());EnchantValues enchant;MobAttackTemplates templates;ActorEquipmentRules rules{items,enchant,templates};Scene scene;Actor actor(42,{}, {},{},scene,rules);actor.attack_state().mode=2;actor.status().skills.equipment.base.attributes.recovery=9;
 Pool allocator;MobDamagePool pool(allocator);MobRuntime mob;MobRuntimeTemplate definition;definition.level=1;definition.experience=100;mob.id=77;mob.definition=&definition;mob.damage=std::make_unique<MobDamageLedger>(pool);mob.hp=10;mob.status=0;mob.target_kind=3;mob.state_stack=std::vector<std::int32_t>{1};mob.lua_ai=1;mob.lua_attacked_count=12;
 MobDeathExperience experience;World world;PartyMembershipUser member;member.id=42;Ai ai;Clock clock;Services services(actor,member,experience,world,ai,clock);if(services.mob_secondary_state(mob)!=3||!services.damage_logging_enabled(mob))throw std::runtime_error("live mob state");mob.lua_ai=0;MobRespawn respawn;respawn.both=true;mob.respawn=&respawn;if(!services.damage_logging_enabled(mob))throw std::runtime_error("linked logging");mob.respawn=nullptr;if(services.damage_logging_enabled(mob))throw std::runtime_error("unexpected logging");mob.lua_ai=1;services.record[0x6c]=std::byte{1};if(!services.long_corpse_delay(mob))throw std::runtime_error("long corpse delay");services.record[0x6c]=std::byte{0};if(services.long_corpse_delay(mob))throw std::runtime_error("short corpse delay");services.record[0x3f]=std::byte{17};mob.position={5,0,7};const auto pos=services.mob_current_position(mob);if(pos.x!=5||pos.z!=7||services.mob_size(mob)!=17||clock.calls!=1)throw std::runtime_error("shared mob position and clock");PartyNormalMembership group;group.id=77;member.party=&group;if(services.party_id(actor)!=77||services.aggro_bonus(actor)!=9)throw std::runtime_error("retained party/recovery");group.id=0;if(services.party_id(actor)!=0)throw std::runtime_error("present zero party ID");member.party=nullptr;if(services.party_id(actor))throw std::runtime_error("absent party");Actor foreign(43,{}, {},{},scene,rules);bool rejected=false;try{services.party_id(foreign);}catch(const std::invalid_argument&){rejected=true;}if(!rejected)throw std::runtime_error("foreign actor accepted");member.id=43;rejected=false;try{services.aggro_bonus(actor);}catch(const std::invalid_argument&){rejected=true;}if(!rejected)throw std::runtime_error("mismatched membership accepted");member.id=42;std::array<std::int32_t,3> damage{10,0,0};
 if(!vertical_combat_hit(actor,mob,nullptr,damage,damage,services)||mob.hp!=0||mob.status!=2||mob.death_time!=5123||mob.target_kind!=0||actor.attack_state().mode!=0||world.events!="FEDQLC"||services.damage_logs!=1||clock.calls!=2||ai.calls!=1||mob.state_stack->back()!=2||mob.function_type!=0||world.experience<=0||!mob.damage->records().empty())throw std::runtime_error("lethal hit to real mob death coordination");
 std::cout<<"lethal live hit through mob death and rewards passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
