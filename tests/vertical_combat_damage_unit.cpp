#include "eternity/game/vertical_combat_damage.hpp"
#include "eternity/game/vertical_combat_ranged_passive.hpp"
#include "eternity/game/mob_combat_attack.hpp"
#include <bit>
#include <cfenv>
#if defined(__x86_64__) || defined(__i386__)
#include <xmmintrin.h>
#endif
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
struct Draw:RandomSource {std::array<int,5> values{};unsigned count=0;std::int32_t next() override {if(count>=values.size())throw std::runtime_error("excess draw");return values[count++];}};
struct Passive:VerticalCombatRangedPassive {std::int32_t value=0;unsigned calls=0;std::int32_t ranged_penalty_suppression(const Actor&) override {++calls;return value;}};
struct ReachedHit {};
struct Combined:VerticalCombatNormalDamageWorld {
 ItemDefinition weapon_definition;Actor& actor;MobRuntime& mob;int sleep_calls=0,expected_damage=121;
 Combined(RandomSource& r,ZoneUsers& z,VerticalCombatRangedPassive& p,std::span<const float,81> e,Actor& a,MobRuntime& m):VerticalCombatNormalDamageWorld(r,z,p,e),actor(a),mob(m){}
 void remove_sleep(MobRuntime& m) override {if(&m!=&mob)throw std::runtime_error("wrong mob");++sleep_calls;}
 const ItemDefinition& weapon(Actor& a) override {if(&a!=&actor)throw std::runtime_error("wrong actor");return weapon_definition;}
 void wear_weapon(Actor&) override {throw std::runtime_error("unexpected wear");}
 bool apply_hit(Actor& a,MobRuntime& m,std::array<std::int32_t,3>& d) override {if(&a!=&actor||&m!=&mob||d!=std::array<std::int32_t,3>{expected_damage,0,0})throw std::runtime_error("actual caller damage mismatch");throw ReachedHit{};}
 void send_attack_view(Actor&,const net::Packet&) override {throw std::runtime_error("unexpected send");}
 void set_target(MobRuntime&,Actor&) override {throw std::runtime_error("unexpected target");}
 void attack_item_option(Actor&,MobRuntime&) override {throw std::runtime_error("unexpected item option");}
};

// Test-only state: MobCombatState is not yet the live MobRuntime owner.
struct HitPool:MobDamagePoolServices {
 std::unique_ptr<MobDamageRecord> construct() override {return std::make_unique<MobDamageRecord>();}
};
struct HitServices:MobCombatAttackServices {
 unsigned stops=0,deaths=0,retargets=0;
 std::int32_t status(const MobCombatState& m) override{return m.state;}
 void dispatch_death_event(MobCombatState&) override{throw std::runtime_error("unexpected event");}
 std::uint32_t tick() override{return 123;}
 void log_damage(MobCombatState&,std::uint32_t) override{throw std::runtime_error("unexpected log");}
 void retarget(MobCombatState&,bool) override{++retargets;}
 void stop_attack(MobCombatAttacker&) override{++stops;}
 void death(MobCombatState&,std::int32_t type,std::uint32_t id) override{if(type!=1||id!=42)throw std::runtime_error("death identity");++deaths;}
};
struct HitWorld:VerticalCombatNormalDamageWorld {
 ItemDefinition item; MobCombatState& state; MobDamageLedger& ledger; HitServices& services;
 MobCombatAttacker attacker; unsigned sends=0,targets=0,options=0; net::Packet packet;
 HitWorld(RandomSource& r,ZoneUsers& z,VerticalCombatRangedPassive& p,std::span<const float,81> e,MobCombatState& s,MobDamageLedger& l,HitServices& v)
  :VerticalCombatNormalDamageWorld(r,z,p,e),state(s),ledger(l),services(v){attacker.id=42;attacker.party=7;}
 void remove_sleep(MobRuntime&) override{}
 const ItemDefinition& weapon(Actor&) override{return item;}
 void wear_weapon(Actor&) override{throw std::runtime_error("unexpected critical");}
 bool apply_hit(Actor&,MobRuntime&,std::array<std::int32_t,3>& damage) override {
  return mob_combat_user_hit(state,ledger,services,attacker,damage,damage[0],0);
 }
 void send_attack_view(Actor&,const net::Packet& p) override{++sends;packet=p;}
 void set_target(MobRuntime&,Actor&) override{++targets;}
 void attack_item_option(Actor&,MobRuntime&) override{++options;}
};

}
int main(int argc,char**argv){try{
 if(argc!=2)throw std::runtime_error("fixture required");std::ifstream input(argv[1]);if(!input)throw std::runtime_error("fixture missing");
 // Original ps_game.exe image at 00542260; production requires the live owner.
 constexpr std::array<std::uint32_t,81> bits={0x3f800000u,0x3f4ccccdu,0x3f4ccccdu,0x3f4ccccdu,0x3f4ccccdu,0x3f333333u,0x3f333333u,0x3f333333u,0x3f333333u,0x3f99999au,0x3f800000u,0x3f000000u,0x3f800000u,0x3fb33333u,0x3f800000u,0x3ecccccdu,0x3f800000u,0x3fa66666u,0x3f99999au,0x3fb33333u,0x3f800000u,0x3f000000u,0x3f800000u,0x3fa66666u,0x3f800000u,0x3ecccccdu,0x3f800000u,0x3f99999au,0x3f800000u,0x3fb33333u,0x3f800000u,0x3f000000u,0x3f800000u,0x3fa66666u,0x3f800000u,0x3ecccccdu,0x3f99999au,0x3f000000u,0x3f800000u,0x3fb33333u,0x3f800000u,0x3ecccccdu,0x3f800000u,0x3fa66666u,0x3f800000u,0x3fa66666u,0x3f800000u,0x3f000000u,0x3f800000u,0x3fcccccdu,0x3f800000u,0x3f000000u,0x3f800000u,0x3fb33333u,0x3fa66666u,0x3fcccccdu,0x3f800000u,0x3f000000u,0x3f800000u,0x3fb33333u,0x3f800000u,0x3f000000u,0x3f800000u,0x3fa66666u,0x3f800000u,0x3fcccccdu,0x3f800000u,0x3f000000u,0x3f800000u,0x3fb33333u,0x3f800000u,0x3f000000u,0x3fa66666u,0x3f000000u,0x3f800000u,0x3fcccccdu,0x3f800000u,0x3f000000u,0x3f800000u,0x3fb33333u,0x3f800000u};std::array<float,81> elements{};for(unsigned i=0;i<81;++i)elements[i]=std::bit_cast<float>(bits[i]);
 ItemDefinitions items;EnchantValues enchant;MobAttackTemplates templates;ActorEquipmentRules rules{items,enchant,templates};std::string line;unsigned count=0;
 while(std::getline(input,line)){if(line.empty()||line[0]=='#')continue;unsigned kind,ul,ml,ae,de,passive,critical,draws,distances;int attack,variance,wis,crit,luck,defense,expected;float distance;Draw random;std::istringstream row(line);row>>kind>>ul>>ml>>attack>>variance>>wis>>crit>>luck>>defense>>ae>>de>>passive>>distance;for(auto& n:random.values)row>>n;row>>critical>>expected>>draws>>distances;if(!row)throw std::runtime_error("bad row");
 Scene scene;EffectStatus state;state.level=ul;state.skills.equipment.base.attributes.wisdom=wis;state.skills.equipment.variance=variance;state.skills.equipment.attack_attribute=ae;state.calculated_attack[kind].attack=attack;state.calculated_attack[kind].critical_attack=crit;Actor actor(42,state,{}, {},scene,rules);MobRuntimeTemplate def;def.level=ml;MobRuntime mob;mob.definition=&def;mob.attribute=de;mob.luck=luck;mob.attack[kind].defense=defense;mob.position.x=distance;ZoneUsers zone(1,1,0);zone.attach(actor,0,0,{});Passive suppression;suppression.value=passive;std::array<std::int32_t,3> damage{99,99,99};
 bool actual=vertical_combat_normal_damage(actor,mob,kind,random,zone,suppression,elements,damage);if(actual!=bool(critical)||damage[0]!=expected||damage[1]||damage[2]||random.count!=draws||distances!=(kind==1&&!passive?(1u+unsigned(critical)):0u)||suppression.calls!=(kind==1?(1u+unsigned(critical)):0u))throw std::runtime_error("case "+std::to_string(count)+" expected damage "+std::to_string(expected)+" actual "+std::to_string(damage[0])+" draws "+std::to_string(random.count)+" expected "+std::to_string(draws));actor.status().skills.cross_road=static_cast<std::int32_t>(passive);VerticalCombatActorRangedPassive retained(actor);Draw retained_rng;retained_rng.values=random.values;std::array<std::int32_t,3> retained_damage{99,99,99};const bool retained_critical=vertical_combat_normal_damage(actor,mob,kind,retained_rng,zone,retained,elements,retained_damage);if(retained_critical!=actual||retained_damage!=damage||retained_rng.count!=random.count)throw std::runtime_error("retained ranged passive damage mismatch");zone.detach(actor.id());++count;}
 if(count!=8192)throw std::runtime_error("incomplete fixture");
 // Original trace through the actual caller and shared success/damage RNG.
 Scene scene;EffectStatus state;state.level=5;state.skills.equipment.base.attributes.wisdom=1000;state.skills.equipment.variance=-1;state.skills.equipment.attack_attribute=8;state.calculated_attack[0].attack=1000;state.calculated_attack[0].accuracy=100;state.calculated_attack[0].evasion=100;Actor actor(42,state,{}, {},scene,rules);MobRuntimeTemplate def;def.level=30;MobRuntime mob;mob.definition=&def;mob.luck=-100;mob.attack[0].accuracy=100;mob.attack[0].evasion=100;ZoneUsers zone(1,1,0);zone.attach(actor,0,0,{});Draw random;random.values={0,23559,3161,0,0};Passive passive;Combined world(random,zone,passive,elements,actor,mob);bool reached=false;try{vertical_combat_attack_mob(actor,mob,world);}catch(const ReachedHit&){reached=true;}if(!reached||random.count!=3||world.sleep_calls!=1)throw std::runtime_error("actual normal caller composition");zone.detach(actor.id());
 // Original execution with CRT seed 19 consumes rolls 100, 15331, 209.
 zone.attach(actor,0,0,{});MicrosoftRandom real_rng(19);Combined real_world(real_rng,zone,passive,elements,actor,mob);real_world.expected_damage=98;reached=false;try{vertical_combat_attack_mob(actor,mob,real_world);}catch(const ReachedHit&){reached=true;}if(!reached||real_rng.state()!=13737300u)throw std::runtime_error("retained CRT RNG composition");zone.detach(actor.id());
 // Compose caller, RNG, calculation and credit/death owner. State and
 // surrounding callbacks are test doubles, not production runtime bindings.
 for(bool lethal:{false,true}) {
  zone.attach(actor,0,0,{});mob.attacked=mob.in_battle=mob.attack_at_moment=false;
  MicrosoftRandom rng(19);HitPool allocator;MobDamagePool pool(allocator);MobDamageLedger ledger(pool);
  MobCombatState hit;hit.hp=lethal?98:200;hit.state=1;HitServices services;
  HitWorld composed(rng,zone,passive,elements,hit,ledger,services);
  vertical_combat_attack_mob(actor,mob,composed);
  if(hit.hp!=(lethal?0u:102u)||services.stops!=unsigned(lethal)||services.deaths!=unsigned(lethal)||
     services.retargets!=unsigned(!lethal)||composed.sends!=1||composed.targets!=unsigned(!lethal)||
     composed.options!=unsigned(!lethal)||ledger.records().size()!=2||rng.state()!=13737300u||
     composed.packet.size()!=17||composed.packet[11]!=98||composed.packet[12]!=0)
   throw std::runtime_error("normal attack credit/death composition");
  if(ledger.records()[0]->type!=5||ledger.records()[0]->id!=7||ledger.records()[0]->damage!=98||
     ledger.records()[1]->type!=1||ledger.records()[1]->id!=42||ledger.records()[1]->party!=7||ledger.records()[1]->damage!=98)
   throw std::runtime_error("normal attack ledger composition");
  zone.detach(actor.id());
 }
 std::fenv_t env;if(std::fegetenv(&env)||std::fesetround(FE_DOWNWARD))throw std::runtime_error("cannot set fenv");std::array<std::int32_t,3> untouched{7,8,9};Draw rejected_rng;bool rejected=false;
 try{(void)vertical_combat_normal_damage(actor,mob,0,rejected_rng,zone,passive,elements,untouched);}catch(const std::logic_error&){rejected=true;}
 if(std::fesetenv(&env))throw std::runtime_error("cannot restore fenv");if(!rejected||rejected_rng.count||untouched!=std::array<std::int32_t,3>{7,8,9})throw std::runtime_error("damage fenv guard");
#if defined(__x86_64__) || defined(__i386__)
 const auto saved_mxcsr=_mm_getcsr();
 for(auto flag:{0x40u,0x8000u}){_mm_setcsr(saved_mxcsr|flag);Draw denormal_rng;bool bad=false;try{(void)vertical_combat_normal_damage(actor,mob,0,denormal_rng,zone,passive,elements,untouched);}catch(const std::logic_error&){bad=true;}_mm_setcsr(saved_mxcsr);if(!bad||denormal_rng.count||untouched!=std::array<std::int32_t,3>{7,8,9})throw std::runtime_error("denormal mode guard");}
#endif
 std::cout<<count<<" original normal attack damage comparisons passed\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
