#include "eternity/game/vertical_combat_range.hpp"
#include "eternity/game/vertical_combat_mob_position.hpp"
#include "eternity/game/item_pool.hpp"
#include "equipment_fixture.hpp"
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
}
struct World:VerticalCombatRangeServices {
 UserPosition pos;std::uint8_t size=0;std::uint32_t now=0;std::string events;Actor* mutate=nullptr;VerticalMotionState* movement=nullptr;ZoneUsers* zone=nullptr;std::array<float,5>* factors=nullptr;
 UserPosition mob_current_position(MobRuntime&)override{events+='P';if(zone)zone->move(mutate->id(),0,0,{10,0,0});return pos;}
 std::uint8_t mob_size(const MobRuntime&)override{return size;}
 std::uint32_t tick_ms()override{events+='T';if(mutate){mutate->status().skills.equipment.range=7;movement->last_tick=now;(*factors)[0]=100;}return now;}
};
struct Interpolated:VerticalCombatMobPositionServices {
 std::array<std::byte,268> definition{};unsigned clocks=0;
 std::uint32_t tick_ms()override{++clocks;return 10;}
 std::uint32_t mob_secondary_state(const MobRuntime&)override{return 0;}
 std::span<const std::byte> mob_definition_record(const MobRuntime&)override{return definition;}
};
int main(int argc,char**argv){try{
 if(argc!=2)throw std::runtime_error("fixture required");std::ifstream input(argv[1]);if(!input)throw std::runtime_error("fixture missing");
 auto items=ItemDefinitions::load(item_definitions());EnchantValues enchant;MobAttackTemplates templates;ActorEquipmentRules rules{items,enchant,templates};Scene scene;
 Actor actor(42,{}, {},{},scene,rules),target(43,{}, {},{},scene,rules);ZoneUsers zone(1,1,0);zone.attach(actor,0,0,{});zone.attach(target,0,0,{});
 VerticalMotionState movement;World world;std::array<float,5> factors{1,2,3,4,5};VerticalCombatRange range(actor,zone,movement,factors,world);MobRuntime mob;
 std::string line;unsigned count=0;
 while(std::getline(input,line)){
  if(line.empty()||line[0]=='#')continue;unsigned ismob,size,expected;int speed,weapon,extra;std::uint32_t now,last;float distance;std::string events;std::istringstream row(line);
  row>>ismob>>speed>>weapon>>extra>>size>>now>>last>>distance>>expected>>events;if(!row)throw std::runtime_error("bad fixture");
  actor.status().skills.equipment.move_speed=speed;actor.status().skills.equipment.range=weapon;movement.last_tick=last;world.now=now;world.size=size;world.pos={distance,0,0};world.events.clear();zone.move(target.id(),0,0,{distance,0,0});
  bool result=ismob?range.mob(actor,mob,extra):range.user(actor,target,extra);
  if(result!=bool(expected)||events!=world.events)throw std::runtime_error("range mismatch case "+std::to_string(count));++count;
 }
 if(count!=360)throw std::runtime_error("incomplete fixture");
 // Mob position changes the live source before distance is sampled; the clock
 // changes the timestamp and weapon after factor/distance have been sampled.
 actor.status().skills.equipment.move_speed=0;actor.status().skills.equipment.range=0;movement.last_tick=0;world.now=5000;world.pos={20,0,0};world.size=0;world.mutate=&actor;world.movement=&movement;world.zone=&zone;world.factors=&factors;world.events.clear();
 if(!range.mob(actor,mob,0)||world.events!="PT")throw std::runtime_error("callback ordering");
 world.mutate=nullptr;world.zone=nullptr;world.factors=nullptr;
 // Use real mob interpolation and definition size in the combat range path.
 Interpolated live;live.definition[0x5c]=std::byte{10};live.definition[0x3f]=std::byte{1};
 mob.status=0;mob.previous_move_time=0;mob.previous_position={10,0,0};mob.move_direction={1,0,0};mob.move_range=2;mob.position={100,0,0};
 zone.move(actor.id(),0,0,{});actor.status().skills.equipment.range=6;movement.last_tick=10;factors[0]=1;
 VerticalCombatRange real_range(actor,zone,movement,factors,live);
 if(real_range.mob(actor,mob,0)||live.clocks!=2)throw std::runtime_error("interpolated range or shared clock");
 actor.status().skills.equipment.range=7;if(!real_range.mob(actor,mob,0)||live.clocks!=4)throw std::runtime_error("interpolated range boundary");
 Actor other(44,{}, {},{},scene,rules);bool rejected=false;try{range.user(other,target,0);}catch(const std::invalid_argument&){rejected=true;}if(!rejected)throw std::runtime_error("foreign source accepted");
 zone.detach(target.id());rejected=false;try{range.user(actor,target,0);}catch(const std::logic_error&){rejected=true;}if(!rejected)throw std::runtime_error("detached target accepted");
 std::cout<<count<<" original range cases passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
