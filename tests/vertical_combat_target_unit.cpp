#include "eternity/game/vertical_combat_target.hpp"
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
struct ZoneWorld:ZoneMobServices,MobTargetingZoneWorld {
 unsigned dispatches=0;Actor* actor=nullptr;MobRuntime* expected=nullptr;
 std::int32_t user_count(const ZoneMobs&) const override{return 1;}
 std::int32_t random(MobRuntime&) override{throw std::runtime_error("unexpected random");}
 std::uint32_t tick_ms(MobRuntime&) override{throw std::runtime_error("unexpected tick");}
 void dispatch(MobRuntime& mob,std::uint32_t event,std::uint32_t argument) override{
  if(&mob!=expected||event!=1||argument!=actor->id()||mob.target_id!=actor->id())throw std::runtime_error("dispatch live target");
  ++dispatches;actor->attack_state().mode=1;
 }
 std::uint32_t tick_ms()override{throw std::runtime_error("unexpected movement");}
 void send_cell_view(CellCoordinates,const net::Packet&)override{}
 std::unique_ptr<MobBroadcast> allocate_broadcast(const net::Packet&,std::optional<std::uint8_t>)override{throw std::runtime_error("unexpected broadcast");}
 void lua_init(MobRuntime&)override{throw std::runtime_error("unexpected init");}
};
struct Random:RandomSource {std::int32_t next()override{throw std::runtime_error("unexpected combat draw");}};
struct Passive:VerticalCombatRangedPassive {std::int32_t ranged_penalty_suppression(const Actor&)override{throw std::runtime_error("unexpected passive");}};
struct Normal:VerticalCombatTargetNormalWorld {
 ItemDefinition item;
 using VerticalCombatTargetNormalWorld::VerticalCombatTargetNormalWorld;
 void remove_sleep(MobRuntime&)override{throw std::runtime_error("unexpected sleep");}
 const ItemDefinition& weapon(Actor&)override{return item;}
 void wear_weapon(Actor&)override{throw std::runtime_error("unexpected wear");}
 bool apply_hit(Actor&,MobRuntime&,std::array<std::int32_t,3>&)override{throw std::runtime_error("unexpected hit");}
 void send_attack_view(Actor&,const net::Packet&)override{throw std::runtime_error("unexpected send");}
 void attack_item_option(Actor&,MobRuntime&)override{throw std::runtime_error("unexpected item");}
};
void put(std::span<std::byte> b,unsigned at,std::uint32_t value,unsigned size=1){for(unsigned i=0;i<size;++i)b[at+i]=static_cast<std::byte>((value>>(8*i))&255);}
}
int main(){try{
 ItemDefinitions items;EnchantValues enchant;MobAttackTemplates templates;ActorEquipmentRules rules{items,enchant,templates};Scene scene;Actor actor(42,{}, {},{},scene,rules);
 auto catalog=MobTargetingBindingCatalog::load(1,1,{},{});auto record=catalog.mutable_mob_record(1);put(record,0,1,2);put(record,38,1);put(record,105,255);put(record,120,1,4);catalog.publish_mob_record(1);
 MobRuntimeTemplate definition;definition.id=1;MobRuntime mob;mob.id=77;mob.definition=&definition;ZoneWorld ports;ports.actor=&actor;ports.expected=&mob;
 ZoneMobs mobs(MapGeometry(1),ports);MobTargetingZone targeting(mobs,catalog,ports);ZoneUsers users(1,1,0);Random random;Passive passive;std::array<float,81> elements{};
 Normal normal(random,users,passive,elements,targeting);
 normal.set_target(mob,actor);
 if(mob.status!=1||mob.target_kind!=1||mob.target_id!=42||ports.dispatches)throw std::runtime_error("actual normal targeting");
 mob.status=0;mob.target_id=0;actor.attack_state().mode=1;normal.set_target(mob,actor);
 if(mob.status!=0||mob.target_id!=0)throw std::runtime_error("dead actor targeted");
 actor.attack_state().mode=0;mob.state_stack=std::vector<std::int32_t>{0};normal.set_target(mob,actor);
 if(mob.target_id!=42||ports.dispatches!=1||actor.attack_state().mode!=1)throw std::runtime_error("actual FSM callback");
 mob.state_stack.reset();mob.status=0;mob.target_id=0;actor.attack_state().mode=0;mob.definition=nullptr;
 bool rejected=false;try{normal.set_target(mob,actor);}catch(const std::invalid_argument&){rejected=true;}if(!rejected)throw std::runtime_error("missing catalog accepted");
 std::cout<<"real actor, target zone, life gate and FSM callback passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
