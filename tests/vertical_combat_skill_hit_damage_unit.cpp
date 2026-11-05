#include "eternity/game/vertical_combat_weapon_wear.hpp"
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
struct World:VerticalCombatSkillHitDamageWorld {
 std::array<std::int32_t,3> initial{},changed{},out{};std::array<std::int32_t,3>* live=nullptr;bool critical=false,first=false;std::string events="C";
 bool calculate_skill_damage(Actor&,MobRuntime&,const UserSkillDefinition&,std::array<std::int32_t,3>& d)override{events+="D";d=initial;live=&d;return critical;}
 void wear_weapon(Actor&)override{events+="W";*live=changed;}
 void finish_skill_hit(Actor&,MobRuntime&,const UserSkillDefinition&,bool f,bool second,bool c,std::array<std::int32_t,3>& d)override{if(f!=first||!second||c!=critical)throw std::runtime_error("continuation flags");events+="B";out=d;}
};
struct Manager:UserSkillRuntimeServices {
 SkillDefinition effect;UserSkillDefinition info{effect,"test"};
 const UserSkillDefinition* definition(std::uint16_t,std::uint8_t)override{return &info;}
 UserSkillHandle allocate()override{return std::make_shared<UserSkillRuntimeSkill>();}
 void destroy(UserSkillHandle)noexcept override{} void strand(UserSkillHandle)noexcept override{}
 void invalid_definition(std::uint16_t,std::uint8_t)override{throw std::runtime_error("missing definition");}
 void disconnect(std::uint32_t,std::uint32_t)override{throw std::runtime_error("disconnect");}
};
struct Passive:VerticalCombatRangedPassive {std::int32_t ranged_penalty_suppression(const Actor&)override{return 0;}};
struct TargetWorld:ZoneMobServices,MobTargetingZoneWorld {
 RandomSource& random_owner;explicit TargetWorld(RandomSource& r):random_owner(r){}
 std::int32_t user_count(const ZoneMobs&)const override{return 1;}
 std::int32_t random_value(MobRuntime&){return random_owner.next();}
 std::int32_t random(MobRuntime& mob)override{return random_value(mob);}
 std::uint32_t tick_ms(MobRuntime&)override{return 0;}
 void dispatch(MobRuntime&,std::uint32_t,std::uint32_t)override{throw std::runtime_error("unexpected target dispatch");}
 std::uint32_t tick_ms()override{return 0;}
 void send_cell_view(CellCoordinates,const net::Packet&)override{}
 std::unique_ptr<MobBroadcast> allocate_broadcast(const net::Packet&,std::optional<std::uint8_t>)override{throw std::runtime_error("unexpected broadcast");}
 void lua_init(MobRuntime&)override{throw std::runtime_error("unexpected init");}
};
void put(std::span<std::byte> b,unsigned at,std::uint32_t v,unsigned n=1){for(unsigned i=0;i<n;++i)b[at+i]=static_cast<std::byte>((v>>(8*i))&255);}
struct Integration:VerticalCombatWearSkillServices {
 std::array<std::uint8_t,255> states{};std::array<std::uint8_t,6> skills{};std::array<std::int32_t,3> output{};std::string events;std::uint8_t raw_mode=1;std::array<std::uint16_t,3> raw_values{100,10,5};
 Integration(RandomSource& r,VerticalCombatSkillRuntimeDamage& d,MobTargetingZone& z,const ItemDefinitions& i):VerticalCombatWearSkillServices(r,d,z,i){}
 VerticalCombatSkillSuccessInputs success_inputs(MobRuntime&,const UserSkillDefinition& i)override{return {i,states,skills,1,100};}
 VerticalCombatSkillRawDamage raw_skill_damage(const UserSkillDefinition&)override{events+='R';return {raw_mode,raw_values};}
 MobAppliedSkillEffectServices& sleep_effect_services(MobRuntime&)override{throw std::runtime_error("unexpected sleep owner lookup");}
 void send_skill_view(Actor&,const net::Packet& p)override{if(p.size()!=20||p[0]!=0x17||p[1]!=5)throw std::runtime_error("primary packet");events+='P';}
 bool apply_primary(Actor&,MobRuntime&,const UserSkillDefinition&,std::array<std::int32_t,3>& d)override{events+='D';output=d;return false;}
 std::array<std::uint16_t,3> secondary_words(const UserSkillDefinition&)override{return {};}
 bool apply_secondary(Actor&,MobRuntime&,const UserSkillDefinition&,std::array<std::int32_t,3>&,std::array<std::int32_t,3>&)override{throw std::runtime_error("unexpected secondary hit");}
 void send_secondary_combat(Actor&,const net::Packet&)override{throw std::runtime_error("unexpected secondary send");}
 void add_mob_skill(MobRuntime&,Actor&,const UserSkillDefinition&)override{throw std::runtime_error("unexpected mob effect");}
 void attack_item_option(Actor&,MobRuntime&)override{events+='I';}
 void announce_multi(Actor&,MobRuntime&,const UserSkillDefinition&)override{throw std::runtime_error("unexpected announcement");}
 bool self_damage(Actor&,std::int32_t)override{throw std::runtime_error("unexpected self damage");}
 void stop_mob_attack(MobRuntime&)override{throw std::runtime_error("unexpected stop");}
 void self_death(Actor&)override{throw std::runtime_error("unexpected death");}
 void remaining_mob_effect(UserSkillAction,Actor&,MobRuntime*,const UserSkillDefinition&)override{throw std::runtime_error("unexpected action");}
 void user_effect(UserSkillAction,Actor&,Actor*,const UserSkillDefinition&)override{throw std::runtime_error("unexpected user effect");}
 void spend(Actor&,std::uint16_t,std::uint16_t)override{throw std::runtime_error("unexpected spend");}
 std::uint32_t tick_ms()override{throw std::runtime_error("unexpected clock");}
};
}
int main(int argc,char**argv){try{
 if(argc!=2)throw std::runtime_error("fixture required");std::ifstream input(argv[1]);if(!input)throw std::runtime_error("missing fixture");
 ItemDefinitions items;EnchantValues enchant;MobAttackTemplates templates;ActorEquipmentRules rules{items,enchant,templates};std::string line;unsigned count=0;
 while(std::getline(input,line)){
  if(line.empty()||line[0]=='#')continue;unsigned first,critical,kind,detail;std::string expected;World world;std::istringstream row(line);
  row>>first>>critical>>kind>>detail;for(auto& v:world.initial)row>>v;for(auto& v:world.changed)row>>v;row>>expected;std::array<std::int32_t,3> out{};for(auto& v:out)row>>v;if(!row)throw std::runtime_error("bad fixture");
  Scene scene;Actor actor(42,{}, {},{},scene,rules);MobRuntime mob;SkillDefinition effect;effect.detail=detail;UserSkillDefinition info{effect,"test"};info.attack_kind=kind;
  world.first=first;world.critical=critical;if(first)world.events+='S';
  vertical_combat_skill_hit_damage(actor,mob,info,first,true,world);
  std::string events;for(char e:world.events){if(!events.empty())events+=',';events+=e;}
  if(events!=expected||world.out!=out)throw std::runtime_error("fixture mismatch "+std::to_string(count));++count;
 }
 if(count!=96)throw std::runtime_error("incomplete fixture");
 Scene scene;EffectStatus state;state.level=5;state.skills.equipment.base.attributes.wisdom=1000;state.skills.equipment.variance=-1;state.skills.equipment.attack_attribute=8;state.calculated_attack[0].attack=1000;state.calculated_attack[0].accuracy=100;state.calculated_attack[0].evasion=100;
 auto actor=std::make_shared<Actor>(42,state,PlayerResources{},std::vector<PersistedEffect>{},scene,rules);Manager manager;manager.effect.id=11;manager.effect.detail=21;manager.effect.effect=1;manager.info.target_kind=3;
 UserSkillRuntime runtime(actor,manager);MobRuntimeTemplate def;def.id=1;def.level=30;MobRuntime mob;mob.definition=&def;mob.luck=-100;mob.attack[0].accuracy=100;mob.attack[0].evasion=100;
 ZoneUsers zone(1,1,0);Passive passive;std::array<float,81> elements;elements.fill(1.0f);elements[72]=1.3f;MicrosoftRandom rng(19);
 auto catalog=MobTargetingBindingCatalog::load(1,1,{},{});auto record=catalog.mutable_mob_record(1);put(record,0,1,2);put(record,38,1);put(record,105,255);put(record,120,1,4);catalog.publish_mob_record(1);
 TargetWorld targeting_world(rng);ZoneMobs mobs(MapGeometry(1),targeting_world);MobTargetingZone targeting(mobs,catalog,targeting_world);
 VerticalCombatSkillRuntimeDamage damage(runtime,rng,zone,passive,elements);Integration world(rng,damage,targeting,items);
 world.mob_effect(UserSkillAction::attack_select,*actor,&mob,manager.info);
 if(world.events!="RDPI"||world.output!=std::array<std::int32_t,3>{104,10,5}||rng.state()!=13737300u||mob.target_kind!=1||mob.target_id!=42||mob.status!=1)throw std::runtime_error("actual selected skill calculation composition");
 actor->replace_resources_raw({11,7,8});world.events.clear();world.raw_mode=3;world.raw_values={2,3,4};world.mob_effect(UserSkillAction::attack_select,*actor,&mob,manager.info);
 if(world.events!="RDPI"||world.output!=std::array<std::int32_t,3>{22,24,28})throw std::runtime_error("selected skill did not use live actor resources");
 MicrosoftRandom other(19);bool rejected=false;try{Integration invalid(other,damage,targeting,items);}catch(const std::invalid_argument&){rejected=true;}if(!rejected)throw std::runtime_error("different RNG accepted");
 std::cout<<count<<" original damage-stage comparisons and retained runtime composition passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
