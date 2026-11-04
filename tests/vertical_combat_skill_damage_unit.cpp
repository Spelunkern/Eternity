#include "eternity/game/vertical_combat_skill_runtime_damage.hpp"
#include "eternity/game/actor_load.hpp"
#include <bit>
#include <cfenv>
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
struct Manager:UserSkillRuntimeServices {
 SkillDefinition effect;UserSkillDefinition info{effect,"retained"};
 const UserSkillDefinition* definition(std::uint16_t,std::uint8_t) override{return &info;}
 UserSkillHandle allocate() override{return std::make_shared<UserSkillRuntimeSkill>();}
 void destroy(UserSkillHandle) noexcept override{} void strand(UserSkillHandle) noexcept override{}
 void invalid_definition(std::uint16_t,std::uint8_t) override{throw std::runtime_error("definition");}
 void disconnect(std::uint32_t,std::uint32_t) override{throw std::runtime_error("disconnect");}
};

}
int main(int argc,char**argv){try{
 if(argc!=2)throw std::runtime_error("fixture required");std::ifstream input(argv[1]);if(!input)throw std::runtime_error("fixture missing");
 constexpr std::array<std::uint32_t,81> bits={0x3f800000u,0x3f4ccccdu,0x3f4ccccdu,0x3f4ccccdu,0x3f4ccccdu,0x3f333333u,0x3f333333u,0x3f333333u,0x3f333333u,0x3f99999au,0x3f800000u,0x3f000000u,0x3f800000u,0x3fb33333u,0x3f800000u,0x3ecccccdu,0x3f800000u,0x3fa66666u,0x3f99999au,0x3fb33333u,0x3f800000u,0x3f000000u,0x3f800000u,0x3fa66666u,0x3f800000u,0x3ecccccdu,0x3f800000u,0x3f99999au,0x3f800000u,0x3fb33333u,0x3f800000u,0x3f000000u,0x3f800000u,0x3fa66666u,0x3f800000u,0x3ecccccdu,0x3f99999au,0x3f000000u,0x3f800000u,0x3fb33333u,0x3f800000u,0x3ecccccdu,0x3f800000u,0x3fa66666u,0x3f800000u,0x3fa66666u,0x3f800000u,0x3f000000u,0x3f800000u,0x3fcccccdu,0x3f800000u,0x3f000000u,0x3f800000u,0x3fb33333u,0x3fa66666u,0x3fcccccdu,0x3f800000u,0x3f000000u,0x3f800000u,0x3fb33333u,0x3f800000u,0x3f000000u,0x3f800000u,0x3fa66666u,0x3f800000u,0x3fcccccdu,0x3f800000u,0x3f000000u,0x3f800000u,0x3fb33333u,0x3f800000u,0x3f000000u,0x3fa66666u,0x3f000000u,0x3f800000u,0x3fcccccdu,0x3f800000u,0x3f000000u,0x3f800000u,0x3fb33333u,0x3f800000u};std::array<float,81> elements{};for(unsigned i=0;i<81;++i)elements[i]=std::bit_cast<float>(bits[i]);
 ItemDefinitions items;EnchantValues enchant;MobAttackTemplates templates;ActorEquipmentRules rules{items,enchant,templates};std::string line;unsigned count=0;
 while(std::getline(input,line)){if(line.empty()||line[0]=='#')continue;
 unsigned kind,mode,detail,override_attr,vals[3],maximum[3],current[3],ul,ml,ae,de,passive,critical,draws,distances;int recovery[3],attack,variance,wis,crit,luck,defense,expected[3];float distance;Draw random;std::istringstream row(line);
 row>>kind>>mode>>detail>>override_attr;for(auto& v:vals)row>>v;for(auto& v:recovery)row>>v;for(auto& v:maximum)row>>v;for(auto& v:current)row>>v;row>>ul>>ml>>attack>>variance>>wis>>crit>>luck>>defense>>ae>>de>>passive>>distance;for(auto& v:random.values)row>>v;row>>critical;for(auto& v:expected)row>>v;row>>draws>>distances;if(!row)throw std::runtime_error("bad row");
 Scene scene;EffectStatus state;state.level=ul;state.skills.equipment.base.attributes.wisdom=wis;state.skills.equipment.variance=variance;state.skills.equipment.attack_attribute=ae;state.calculated_attack[kind].attack=attack;state.calculated_attack[kind].critical_attack=crit;Actor actor(42,state,{}, {},scene,rules);MobRuntimeTemplate def;def.level=ml;def.hp=maximum[0];def.mp=maximum[1];def.sp=maximum[2];MobRuntime mob;mob.definition=&def;mob.hp=current[0];mob.mp=current[1];mob.sp=current[2];mob.attribute=de;mob.luck=luck;mob.attack[kind].defense=defense;mob.position.x=distance;ZoneUsers zone(1,1,0);zone.attach(actor,0,0,{});Passive suppression;suppression.value=passive;SkillDefinition effect;effect.detail=detail;effect.attribute=override_attr;UserSkillDefinition skill{effect,{}};std::array<std::int32_t,3> live_recovery{recovery[0],recovery[2],recovery[1]};VerticalCombatSkillDamageInputs binding{skill,static_cast<std::uint8_t>(mode),{static_cast<std::uint16_t>(vals[0]),static_cast<std::uint16_t>(vals[1]),static_cast<std::uint16_t>(vals[2])},live_recovery};std::array<std::int32_t,3> damage{99,99,99};
 const bool weapon=mode==1||mode==2||mode>5;const auto expected_distance=weapon&&kind&&!passive?1u+unsigned(critical):0u;const auto expected_passive=weapon&&kind?1u+unsigned(critical):0u;
 bool actual=vertical_combat_skill_damage(actor,mob,kind,random,zone,suppression,elements,binding,damage);if(distances!=expected_distance||suppression.calls!=expected_passive||actual!=bool(critical)||damage[0]!=expected[0]||damage[1]!=expected[1]||damage[2]!=expected[2]||random.count!=draws)throw std::runtime_error("case "+std::to_string(count)+" mode "+std::to_string(mode)+" damage "+std::to_string(damage[0])+","+std::to_string(damage[1])+","+std::to_string(damage[2])+" expected "+std::to_string(expected[0])+","+std::to_string(expected[1])+","+std::to_string(expected[2])+" draws "+std::to_string(random.count)+"/"+std::to_string(draws));zone.detach(actor.id());++count;}
 if(count!=6144)throw std::runtime_error("incomplete fixture");
 // Actual retained ActorLoad result, UserSkillRuntime and captured skill definition.
 Scene scene;auto loaded=std::make_shared<LoadedActor>();loaded->actor=std::make_unique<Actor>(42,EffectStatus{},PlayerResources{},std::vector<PersistedEffect>{},scene,rules);loaded->attack_recovery={10,20,30};std::shared_ptr<Actor> actor(loaded,loaded->actor.get());Manager manager;UserSkillRuntime runtime(actor,manager);const net::Packet learned{4,5,42,0,0,0,1,11,0,1,0,0,0};if(runtime.load(learned,0)!=UserSkillLoadResult::loaded)throw std::runtime_error("runtime load");const auto captured=runtime.slot(0);if(!captured||!captured->definition)throw std::runtime_error("missing retained skill");MobRuntime mob;ZoneUsers zone(1,1,0);Draw random;Passive passive;VerticalCombatSkillRuntimeDamage adapter(runtime,random,zone,passive,elements,loaded->attack_recovery);std::array<std::int32_t,3> damage{};
 if(adapter.calculate(*actor,mob,*captured->definition,3,{2,3,4},damage)||damage!=std::array<std::int32_t,3>{20,90,80}||random.count)throw std::runtime_error("retained skill/recovery composition");loaded->attack_recovery[0]=11;if(adapter.calculate(*actor,mob,*captured->definition,3,{2,3,4},damage)||damage[0]!=22)throw std::runtime_error("live recovery reload");
 // Original AttackSuccess -> AttackDamage trace on the same real CRT owner.
 actor->status().level=5;actor->status().skills.equipment.base.attributes.wisdom=1000;actor->status().skills.equipment.variance=-1;actor->status().skills.equipment.attack_attribute=8;actor->status().calculated_attack[0].attack=1000;actor->status().calculated_attack[0].accuracy=100;actor->status().calculated_attack[0].evasion=100;manager.effect.id=11;manager.effect.detail=21;MobRuntimeTemplate mobdef;mobdef.level=30;mob.definition=&mobdef;mob.luck=-100;mob.attack[0].accuracy=100;mob.attack[0].evasion=100;
 std::array<std::uint8_t,255> state_immunity{};std::array<std::uint8_t,6> skill_immunity{};VerticalCombatSkillSuccessInputs success_inputs{*captured->definition,state_immunity,skill_immunity,1,100};MicrosoftRandom real_rng(19);VerticalCombatSkillRuntimeDamage shared(runtime,real_rng,zone,passive,elements,loaded->attack_recovery);
 if(!vertical_combat_skill_success(*actor,mob,0,success_inputs,real_rng)||shared.calculate(*actor,mob,*captured->definition,1,{100,10,5},damage)||damage!=std::array<std::int32_t,3>{104,10,5}||real_rng.state()!=13737300u)throw std::runtime_error("shared CRT skill pipeline");
 // Integer-only branches must not require a floating environment; percent
 // branches reject unsupported rounding before touching output or randomness.
 std::fenv_t env;if(std::fegetenv(&env)||std::fesetround(FE_DOWNWARD))throw std::runtime_error("cannot set fenv");damage={7,8,9};Draw alternate_rng;VerticalCombatSkillRuntimeDamage alternate(runtime,alternate_rng,zone,passive,elements,loaded->attack_recovery);
 if(alternate.calculate(*actor,mob,*captured->definition,0,{65535,2,3},damage)||damage!=std::array<std::int32_t,3>{65535,2,3})throw std::runtime_error("integer mode under alternate rounding");
 bool bad_env=false;try{(void)alternate.calculate(*actor,mob,*captured->definition,4,{1,0,0},damage);}catch(const std::logic_error&){bad_env=true;}
 if(std::fesetenv(&env))throw std::runtime_error("cannot restore fenv");if(!bad_env||alternate_rng.count||damage!=std::array<std::int32_t,3>{65535,2,3})throw std::runtime_error("percentage environment guard");
 manager.info.attack_kind=2;bool bad_magic=false;try{(void)adapter.calculate(*actor,mob,*captured->definition,1,{1,2,3},damage);}catch(const std::invalid_argument&){bad_magic=true;}if(!bad_magic)throw std::runtime_error("unsupported magic weapon mode");manager.info.attack_kind=0;
 Actor other(43,{}, {},{},scene,rules);bool rejected=false;try{(void)adapter.calculate(other,mob,*captured->definition,0,{1,2,3},damage);}catch(const std::invalid_argument&){rejected=true;}if(!rejected)throw std::runtime_error("wrong actor accepted");
 std::cout<<count<<" original skill damage comparisons passed\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
