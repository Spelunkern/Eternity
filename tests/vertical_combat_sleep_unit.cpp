#include "eternity/game/vertical_combat_sleep.hpp"
#include "eternity/game/mob_applied_skills_runtime.hpp"
#include "eternity/game/mob_pool.hpp"
#include "mob_damage_fixture.hpp"
#include <atomic>
#include <iostream>
#include <stdexcept>
using namespace eternity;using namespace eternity::game;
namespace {
struct Skills:MobAppliedSkillPoolServices {std::unique_ptr<MobAppliedSkill> construct()override{return std::make_unique<MobAppliedSkill>();}};
struct Services:MobPoolServices,MobAppliedSkillsRuntimeServices,MobAppliedSkillsCatalogPorts,MobAppliedSkillEffectServices {
 MobDamageFixture damage;MobRuntimeTemplate& def;unsigned speed=0,removed=0;MobRuntime* subject=nullptr;
 explicit Services(MobRuntimeTemplate& d):def(d){}
 std::unique_ptr<MobRuntime> construct()override{auto m=std::make_unique<MobRuntime>();m->definition=&def;return m;}
 std::uint32_t unix_time()override{return 123;} MobDamagePool& damage_pool()override{return damage.pool;}
 void release_compiled_ai(MobRuntime&)override{} void release_stack_ai(MobRuntime&)override{} void close_lua(void*)override{}
 MobAppliedSkillsCatalogPorts& ports(MobRuntime&)override{return *this;}
 std::uint32_t tick()noexcept override{return 100;}
 void log(const MobAppliedSkill&,std::string_view,std::uint8_t action)noexcept override{if(action==6)++removed;}
 void rebuild_attack(MobRuntime&)noexcept override{std::terminate();}
 void send_speed(MobRuntime& m)noexcept override{if(&m!=subject)std::terminate();++speed;}
 bool damage_periodic(MobRuntime&,std::uint32_t,std::uint32_t,MobAppliedSkillDamage&)noexcept override{std::terminate();}
 bool damage_hp(MobRuntime&,std::uint32_t,std::uint32_t,std::uint32_t)noexcept override{std::terminate();}
 void death(MobRuntime&,std::uint32_t,std::uint32_t)noexcept override{std::terminate();}
 void send_result(MobRuntime&,const MobAppliedSkillDamagePacket&,float,std::uint8_t)noexcept override{std::terminate();}
 void finish_damage_skill(MobRuntime&,MobAppliedSkill&,const SkillDefinition&)noexcept override{std::terminate();}
};
struct ZonePorts:ZoneMobServices,MobTargetingZoneWorld {
 std::int32_t user_count(const ZoneMobs&)const override{return 0;}
 std::int32_t random(MobRuntime&)override{throw std::runtime_error("unexpected random");}
 std::uint32_t tick_ms(MobRuntime&)override{throw std::runtime_error("unexpected tick");}
 void dispatch(MobRuntime&,std::uint32_t,std::uint32_t)override{throw std::runtime_error("unexpected dispatch");}
 std::uint32_t tick_ms()override{return 0;}void send_cell_view(CellCoordinates,const net::Packet&)override{}
 std::unique_ptr<MobBroadcast> allocate_broadcast(const net::Packet&,std::optional<std::uint8_t>)override{throw std::runtime_error("unexpected broadcast");}
 void lua_init(MobRuntime&)override{throw std::runtime_error("unexpected init");}
};
struct Random:RandomSource{std::int32_t next()override{throw std::runtime_error("unexpected draw");}};
struct Passive:VerticalCombatRangedPassive{std::int32_t ranged_penalty_suppression(const Actor&)override{return 0;}};
struct Normal:VerticalCombatSleepNormalWorld {
 Services& effects;unsigned providers=0;ItemDefinition item;
 Normal(RandomSource& r,ZoneUsers& z,VerticalCombatRangedPassive& p,std::span<const float,81> e,MobTargetingZone& t,Services& v):VerticalCombatSleepNormalWorld(r,z,p,e,t),effects(v){}
 MobAppliedSkillEffectServices& sleep_effect_services(MobRuntime&)override{++providers;return effects;}
 const ItemDefinition& weapon(Actor&)override{return item;}
 void wear_weapon(Actor&)override{throw std::runtime_error("unexpected wear");}
 bool apply_hit(Actor&,MobRuntime&,std::array<std::int32_t,3>&)override{throw std::runtime_error("unexpected hit");}
 void send_attack_view(Actor&,const net::Packet&)override{throw std::runtime_error("unexpected send");}
 void attack_item_option(Actor&,MobRuntime&)override{throw std::runtime_error("unexpected item");}
};
void put(std::span<std::byte> b,unsigned at,std::uint32_t value,unsigned size=1){for(unsigned i=0;i<size;++i)b[at+i]=static_cast<std::byte>((value>>(8*i))&255);}
void load(MobTargetingBindingCatalog& c,unsigned id,unsigned detail){auto b=c.mutable_skill_record(id);put(b,0,id,2);put(b,2,100);put(b,49,3);put(b,50,detail,2);put(b,92,10,4);b[3]=std::byte{'S'};c.publish_skill_record(id);}
}
int main(){try{
 auto catalog=MobTargetingBindingCatalog::load(0,2,{},{});load(catalog,1,0x42);load(catalog,2,0x40);
 MobRuntimeTemplate definition;definition.id=1;Skills skill_factory;MobAppliedSkillPool skill_pool(skill_factory);Services services(definition);std::atomic<std::uint32_t> skill_ids{0};
 MobAppliedSkillsRuntime runtime(catalog,skill_pool,skill_ids,services);std::uint32_t mob_ids=0;MobPool pool(mob_ids,services,runtime);MobRuntime* mob=pool.allocate();services.subject=mob;mob->lua_ai=1;
 if(!mob->applied_skills().apply(1,1,42)||!mob->applied_skills().apply(2,1,42))throw std::runtime_error("skill setup");
 const auto before=mob->applied_skills().owner().records();if(before.size()!=2||mob->sleep!=1||mob->stun!=1)throw std::runtime_error("live setup");
 ZonePorts zone_ports;ZoneMobs mobs(MapGeometry(1),zone_ports);MobTargetingZone target(mobs,catalog,zone_ports);ZoneUsers users(1,1,0);Random random;Passive passive;std::array<float,81> elements{};Normal world(random,users,passive,elements,target,services);
 mob->sleep=0;const auto original_speed=services.speed;world.remove_sleep(*mob);
 if(world.providers||mob->applied_skills().owner().records()!=before||services.speed!=original_speed)throw std::runtime_error("zero sleep must not touch owner");
 mob->sleep=1;world.remove_sleep(*mob);const auto after=mob->applied_skills().owner().records();
 if(world.providers!=1||mob->sleep!=0||mob->stun!=1||after.size()!=1||after[0]!=before[1]||skill_pool.available().size()!=1||skill_pool.available()[0]!=before[0]||services.removed!=1||services.speed!=original_speed+1)throw std::runtime_error("actual sleep removal");
 MobRuntime unbound;world.remove_sleep(unbound);if(world.providers!=1)throw std::runtime_error("unbound zero sleep");unbound.sleep=1;
 bool rejected=false;try{world.remove_sleep(unbound);}catch(const std::logic_error&){rejected=true;}if(!rejected||world.providers!=1)throw std::runtime_error("unbound sleep accepted");
 std::cout<<"retained mob skill pool sleep removal, logging and speed delivery passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
