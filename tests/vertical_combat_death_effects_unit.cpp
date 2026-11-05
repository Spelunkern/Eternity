#include "eternity/game/vertical_combat_death.hpp"
#include "eternity/game/vertical_combat_skill_effect.hpp"
#include "equipment_fixture.hpp"
#include "eternity/game/mob_applied_skills_runtime.hpp"
#include "eternity/game/mob_pool.hpp"
#include "mob_damage_fixture.hpp"
#include <atomic>
#include <iostream>
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
struct Slots:VerticalCombatSkillSlotServices {
 std::optional<VerticalCombatSkillSlot> binding;
 std::optional<VerticalCombatSkillSlot> resolve(const MobRuntime&,const UserSkillDefinition&)override{return binding;}
};
[[noreturn]] void unexpected(){throw std::runtime_error("unexpected death service");}
struct World:VerticalCombatDeathAppliedSkillsWorld {
 unsigned death_logs=0;
 void send_death(MobRuntime&,const net::Packet&,std::uint8_t)override{unexpected();}
 MobDeathUser* find_user(MobRuntime&,std::uint32_t)override{unexpected();}MobDeathParty* party(MobDeathUser&)override{unexpected();}
 void lock_party(MobDeathParty&)override{unexpected();}void unlock_party(MobDeathParty&)noexcept override{}
 bool in_reward_range(MobDeathUser&,MobDeathUser&,std::int32_t)override{unexpected();}void add_experience(MobDeathUser&,std::uint32_t,std::int32_t)override{unexpected();}
 void drop_items(MobRuntime&,MobDeathUser*,MobDeathParty*)override{unexpected();}void quest_kill(MobDeathUser&,MobRuntime&)override{unexpected();}
 std::optional<std::int32_t> obelisk_country(MobRuntime&,std::int32_t)override{unexpected();}void destroy_obelisk(MobRuntime&,std::int32_t,std::int32_t)override{unexpected();}
 void destroy_linked_group(MobRuntime&,std::int32_t)override{unexpected();}void remove_obelisk_member(MobRuntime&,void*)override{unexpected();}void remove_linked_member(MobRuntime&,void*)override{unexpected();}
 void log_death(MobRuntime& mob,MobDeathUser*,bool fair)override{if(fair||mob.damage->records().empty()||mob.applied_skills().owner().records().empty())unexpected();++death_logs;}
};
struct Skills:MobAppliedSkillPoolServices{std::unique_ptr<MobAppliedSkill> construct()override{return std::make_unique<MobAppliedSkill>();}};
struct Services:MobPoolServices,MobAppliedSkillsRuntimeServices,MobAppliedSkillsCatalogPorts {
 MobDamageFixture damage_heap;MobRuntimeTemplate definition;MobRuntime* subject=nullptr;unsigned removed=0;bool visible=true;
 std::unique_ptr<MobRuntime> construct()override{auto m=std::make_unique<MobRuntime>();m->definition=&definition;return m;}
 std::uint32_t unix_time()override{return 100;}MobDamagePool& damage_pool()override{return damage_heap.pool;}
 void release_compiled_ai(MobRuntime&)override{}void release_stack_ai(MobRuntime&)override{unexpected();}void close_lua(void*)override{unexpected();}
 MobAppliedSkillsCatalogPorts& ports(MobRuntime&)override{return *this;}std::uint32_t tick()noexcept override{return 100;}
 void log(const MobAppliedSkill& s,std::string_view,std::uint8_t action)noexcept override{
  if(action==6){++removed;if(!subject||subject->damage->records().size()!=0||subject->applied_skills().owner().records().empty()||subject->applied_skills().owner().records()[0]!=&s)visible=false;}
 }
 void rebuild_attack(MobRuntime&)noexcept override{}void send_speed(MobRuntime&)noexcept override{}
 bool damage_periodic(MobRuntime&,std::uint32_t,std::uint32_t,MobAppliedSkillDamage&)noexcept override{std::terminate();}
 bool damage_hp(MobRuntime&,std::uint32_t,std::uint32_t,std::uint32_t)noexcept override{std::terminate();}
 void death(MobRuntime&,std::uint32_t,std::uint32_t)noexcept override{std::terminate();}
 void send_result(MobRuntime&,const MobAppliedSkillDamagePacket&,float,std::uint8_t)noexcept override{std::terminate();}
};
void put(std::span<std::byte> b,unsigned at,unsigned v,unsigned n){for(unsigned i=0;i<n;++i)b[at+i]=std::byte((v>>(i*8))&255);}
}
int main(){try{
 auto catalog=MobTargetingBindingCatalog::load(0,1,{},{});auto record=catalog.mutable_skill_record(1);put(record,0,1,2);put(record,2,100,1);record[3]=std::byte{'S'};put(record,49,3,1);put(record,50,0x40,2);put(record,92,10,4);catalog.publish_skill_record(1);
 Skills skill_factory;MobAppliedSkillPool skill_pool(skill_factory);Services services;std::atomic<std::uint32_t> skill_ids{0};MobAppliedSkillsRuntime runtime(catalog,skill_pool,skill_ids,services);std::uint32_t mob_ids=0;MobPool pool(mob_ids,services,runtime);auto* mob=pool.allocate();services.subject=mob;mob->lua_ai=1;
 auto items=ItemDefinitions::load(item_definitions());EnchantValues enchant;MobAttackTemplates templates;ActorEquipmentRules equipment{items,enchant,templates};Scene scene;Actor actor(42,{}, {},{},scene,equipment);
 const auto* definition=catalog.skill_definition(1);if(!definition)throw std::runtime_error("missing skill");UserSkillDefinition info{*definition,"S"};Slots slots;slots.binding=VerticalCombatSkillSlot{1,&catalog};
 vertical_combat_add_mob_skill(*mob,actor,info,slots);
 if(mob->applied_skills().owner().records().size()!=1||mob->applied_skills().owner().records()[0]->target_type!=1||mob->applied_skills().owner().records()[0]->target_id!=42)throw std::runtime_error("retained skill admission");
 SkillDefinition foreign=*definition;UserSkillDefinition foreign_info{foreign,"S"};slots.binding=VerticalCombatSkillSlot{1,&catalog};bool rejected=false;try{vertical_combat_add_mob_skill(*mob,actor,foreign_info,slots);}catch(const std::logic_error&){rejected=true;}if(!rejected||mob->applied_skills().owner().records().size()!=1)throw std::runtime_error("foreign skill identity accepted");
 slots.binding=VerticalCombatSkillSlot{2,&catalog};rejected=false;try{vertical_combat_add_mob_skill(*mob,actor,info,slots);}catch(const std::logic_error&){rejected=true;}if(!rejected)throw std::runtime_error("absent skill slot accepted");
 auto* effect=mob->applied_skills().owner().records()[0];mob->damage->add(1,42,10,0,0);mob->damage->fair_combat=false;
 World world;MobDeathExperience rules;mob_death(*mob,1,42,rules,world);
 if(!mob->damage->records().empty()||!mob->applied_skills().owner().records().empty()||skill_pool.available().size()!=1||skill_pool.available()[0]!=effect||services.removed!=1||world.death_logs!=1||!services.visible)throw std::runtime_error("retained death skill cleanup");
 mob->lua_ai=0;MobRuntime unbound;rejected=false;try{world.clear_applied_skills(unbound);}catch(const std::logic_error&){rejected=true;}if(!rejected)throw std::runtime_error("unbound skill owner accepted");
 std::cout<<"actual pooled death skill cleanup passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
