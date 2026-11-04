#include "eternity/game/vertical_combat_success.hpp"
#include <fstream>
#include <cfenv>
#if defined(__x86_64__) || defined(__i386__)
#include <xmmintrin.h>
#endif
#include <iostream>
#include <limits>
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
struct Draw:RandomSource {int value=0,calls=0;std::int32_t next() override {++calls;return value;}};
struct Combined:VerticalCombatNormalSuccessWorld {
 ItemDefinition item;int damage_calls=0,packets=0;explicit Combined(Draw& r):VerticalCombatNormalSuccessWorld(r){}
 void remove_sleep(MobRuntime&) override {} const ItemDefinition& weapon(Actor&) override{return item;}
 bool attack_damage(Actor&,MobRuntime&,unsigned,std::array<std::int32_t,3>&) override {++damage_calls;throw std::runtime_error("unexpected calculation owner");}
 void wear_weapon(Actor&) override {throw std::runtime_error("unexpected wear");}
 bool apply_hit(Actor&,MobRuntime&,std::array<std::int32_t,3>&) override {throw std::runtime_error("unexpected damage owner");}
 void send_attack_view(Actor&,const net::Packet&) override {++packets;}
 void set_target(MobRuntime&,Actor&) override {} void attack_item_option(Actor&,MobRuntime&) override {}
};
}
int main(int argc,char**argv){try{
 if(argc!=2)throw std::runtime_error("fixture required");std::ifstream input(argv[1]);if(!input)throw std::runtime_error("fixture missing");
 ItemDefinitions items;EnchantValues enchant;MobAttackTemplates templates;ActorEquipmentRules rules{items,enchant,templates};std::string line;unsigned count=0;
 while(std::getline(input,line)){if(line.empty()||line[0]=='#')continue;unsigned kind,unbeatable,status,ul,ml,expected,draws;int ua,ue,ma,me,dex,mobdex,roll;std::istringstream row(line);row>>kind>>unbeatable>>status>>ul>>ml>>ua>>ue>>ma>>me>>dex>>mobdex>>roll>>expected>>draws;if(!row)throw std::runtime_error("bad row");
 Scene scene;EffectStatus state;state.level=ul;state.skills.equipment.base.attributes.dexterity=dex;state.calculated_attack[kind].accuracy=ua;state.calculated_attack[kind].evasion=ue;Actor actor(42,state,{}, {},scene,rules);MobRuntimeTemplate def;def.level=ml;def.dexterity=mobdex;MobRuntime mob;mob.definition=&def;mob.unbeatable=unbeatable;mob.status=status;mob.attack[kind].accuracy=ma;mob.attack[kind].evasion=me;Draw random;random.value=roll;
 bool actual=vertical_combat_normal_success(actor,mob,kind,random);if(actual!=bool(expected)||random.calls!=int(draws))throw std::runtime_error("case "+std::to_string(count)+" expected "+std::to_string(expected)+" actual "+std::to_string(actual));++count;}
 if(count!=8192)throw std::runtime_error("incomplete fixture");
 Scene scene;EffectStatus state;state.level=30;state.calculated_attack[0].accuracy=100;state.calculated_attack[0].evasion=100;Actor actor(42,state,{}, {},scene,rules);MobRuntimeTemplate def;def.level=30;MobRuntime mob;mob.definition=&def;mob.hp=20;mob.attack[0].accuracy=100;mob.attack[0].evasion=100;Draw random;Combined combined(random);
 random.value=99;vertical_combat_attack_mob(actor,mob,combined);if(random.calls!=1||combined.damage_calls!=0||mob.hp!=20||mob.attacked)throw std::runtime_error("caller composition");
 random.calls=0;if(!vertical_combat_normal_success(actor,mob,999,random)||random.calls)throw std::runtime_error("unknown kind guard");
 mob.state_stack=std::vector<std::int32_t>{3};random.calls=0;vertical_combat_attack_mob(actor,mob,combined);if(random.calls||combined.damage_calls!=0||combined.packets!=2)throw std::runtime_error("live stacked guard");
 // Original 53-bit control produces 69; original extended precision produces 70.
 EffectStatus precision_state;precision_state.level=30;precision_state.calculated_attack[0].accuracy=100;precision_state.calculated_attack[0].evasion=50;Actor precision_actor(44,precision_state,{}, {},scene,rules);MobRuntimeTemplate precision_def;precision_def.level=60;MobRuntime precision_mob;precision_mob.definition=&precision_def;precision_mob.attack[0].accuracy=50;precision_mob.attack[0].evasion=50;Draw precision_rng;precision_rng.value=69;
 if(vertical_combat_normal_success(precision_actor,precision_mob,0,precision_rng)||precision_rng.calls!=1)throw std::runtime_error("53-bit precision boundary");
 // The same original ranged attack changes from miss to hit under downward rounding.
 EffectStatus rounding_state;rounding_state.level=65535;rounding_state.calculated_attack[1].accuracy=0;rounding_state.calculated_attack[1].evasion=100;Actor rounding_actor(45,rounding_state,{}, {},scene,rules);MobRuntimeTemplate rounding_def;rounding_def.level=0;MobRuntime rounding_mob;rounding_mob.definition=&rounding_def;rounding_mob.attack[1].accuracy=std::numeric_limits<std::int32_t>::min();rounding_mob.attack[1].evasion=10;Draw rounding_rng;rounding_rng.value=59;
 if(vertical_combat_normal_success(rounding_actor,rounding_mob,1,rounding_rng)||rounding_rng.calls!=1)throw std::runtime_error("nearest rounding boundary");
 // Zero denominators produce x87 integer-indefinite before the category clamp.
 EffectStatus zero;Actor zero_actor(43,zero,{}, {},scene,rules);MobRuntimeTemplate zero_def;MobRuntime zero_mob;zero_mob.definition=&zero_def;Draw zero_rng;
 zero_rng.value=19;if(!vertical_combat_normal_success(zero_actor,zero_mob,0,zero_rng)||zero_rng.calls!=1)throw std::runtime_error("nonfinite melee below clamp");
 zero_rng.value=20;if(vertical_combat_normal_success(zero_actor,zero_mob,0,zero_rng)||zero_rng.calls!=2)throw std::runtime_error("nonfinite melee clamp boundary");
 zero_rng.value=0;if(vertical_combat_normal_success(zero_actor,zero_mob,2,zero_rng)||zero_rng.calls!=3)throw std::runtime_error("nonfinite magic below clamp");
 zero_rng.value=1;if(!vertical_combat_normal_success(zero_actor,zero_mob,2,zero_rng)||zero_rng.calls!=4)throw std::runtime_error("nonfinite magic clamp boundary");
 std::fenv_t original_env;if(std::fegetenv(&original_env))throw std::runtime_error("cannot save fenv");
 for(int mode:{FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO}){
   if(std::fesetround(mode))throw std::runtime_error("cannot set rounding");Draw draw;bool rejected=false;
   try{(void)vertical_combat_normal_success(zero_actor,zero_mob,0,draw);}catch(const std::logic_error&){rejected=true;}
   if(!rejected||draw.calls||std::fegetround()!=mode)throw std::runtime_error("rounding rejection");
   zero_mob.unbeatable=1;if(vertical_combat_normal_success(zero_actor,zero_mob,0,draw)||draw.calls)throw std::runtime_error("guard before fenv");zero_mob.unbeatable=0;
   if(!vertical_combat_normal_success(zero_actor,zero_mob,9,draw)||draw.calls)throw std::runtime_error("unsupported kind before fenv");
 }
 if(std::fesetenv(&original_env))throw std::runtime_error("cannot restore fenv");
#if defined(__GLIBC__)
 std::feclearexcept(FE_ALL_EXCEPT);if(::feenableexcept(FE_DIVBYZERO)<0)throw std::runtime_error("cannot enable exception");Draw trap_rng;bool trap_rejected=false;
 try{(void)vertical_combat_normal_success(zero_actor,zero_mob,0,trap_rng);}catch(const std::logic_error&){trap_rejected=true;}
 if(std::fesetenv(&original_env))throw std::runtime_error("cannot restore fenv");if(!trap_rejected||trap_rng.calls)throw std::runtime_error("exception mask rejection");
#endif
#if defined(__x86_64__) || defined(__i386__)
 const auto saved_mxcsr=_mm_getcsr();
 for(const auto altered:{(saved_mxcsr & ~0x6000u)|0x2000u,saved_mxcsr & ~0x200u}){
  _mm_setcsr(altered);Draw mxdraw;bool rejected=false;try{(void)vertical_combat_normal_success(zero_actor,zero_mob,0,mxdraw);}catch(const std::logic_error&){rejected=true;}_mm_setcsr(saved_mxcsr);
  if(!rejected||mxdraw.calls)throw std::runtime_error("independent SSE environment rejection");
 }
#endif
 std::cout<<count<<" original success comparisons passed\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
