#include "eternity/game/vertical_combat_success.hpp"
#include <fstream>
#include <cfenv>
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
struct Draw:RandomSource {int value=0,calls=0;std::int32_t next() override {++calls;return value;}};
}
int main(int argc,char**argv){try{
 if(argc!=2)throw std::runtime_error("fixture required");std::ifstream input(argv[1]);if(!input)throw std::runtime_error("fixture missing");
 ItemDefinitions items;EnchantValues enchant;MobAttackTemplates templates;ActorEquipmentRules rules{items,enchant,templates};std::string line;unsigned count=0;
 while(std::getline(input,line)){if(line.empty()||line[0]=='#')continue;unsigned kind,unbeatable,status,st,si,id,im,ht,hv,dex,wis,md,mw,roll,expected,draws;std::istringstream row(line);row>>kind>>unbeatable>>status>>st>>si>>id>>im>>ht>>hv>>dex>>wis>>md>>mw>>roll>>expected>>draws;if(!row)throw std::runtime_error("bad row");
 Scene scene;EffectStatus state;state.skills.equipment.base.attributes.dexterity=dex;state.skills.equipment.base.attributes.wisdom=wis;Actor actor(42,state,{}, {},scene,rules);MobRuntimeTemplate def;def.dexterity=md;def.wisdom=mw;MobRuntime mob;mob.definition=&def;mob.unbeatable=unbeatable;mob.status=status;SkillDefinition effect;effect.id=id;UserSkillDefinition skill{effect,{}};skill.state_type=st;std::array<std::uint8_t,255> states{};std::array<std::uint8_t,6> skills{};if(st)states[st-1]=si;if(im&&id<256)skills[0]=id;VerticalCombatSkillSuccessInputs inputs{skill,states,skills,static_cast<std::uint8_t>(ht),static_cast<std::uint8_t>(hv)};Draw random;random.value=roll;
 bool actual=vertical_combat_skill_success(actor,mob,kind,inputs,random);if(actual!=bool(expected)||random.calls!=int(draws))throw std::runtime_error("case "+std::to_string(count));++count;}
 if(count!=4096)throw std::runtime_error("incomplete fixture");
 Scene scene;EffectStatus state;Actor actor(42,state,{}, {},scene,rules);MobRuntimeTemplate def;MobRuntime mob;mob.definition=&def;SkillDefinition effect;effect.id=1;UserSkillDefinition skill{effect,{}};std::array<std::uint8_t,6> immunity{};VerticalCombatSkillSuccessInputs inputs{skill,{},immunity,3,50};Draw random;bool fault=false;
 try{(void)vertical_combat_skill_success(actor,mob,0,inputs,random);}catch(const std::domain_error&){fault=true;}if(!fault||random.calls)throw std::runtime_error("division fault before draw");
 skill.state_type=1;bool missing=false;try{(void)vertical_combat_skill_success(actor,mob,0,inputs,random);}catch(const std::invalid_argument&){missing=true;}if(!missing||random.calls)throw std::runtime_error("missing immunity before draw");
 skill.state_type=0;inputs.hit_type=1;inputs.hit_value=50;random.value=10;std::fenv_t env;if(std::fegetenv(&env)||std::fesetround(FE_DOWNWARD))throw std::runtime_error("cannot set fenv");
 if(!vertical_combat_skill_success(actor,mob,0,inputs,random)||random.calls!=1)throw std::runtime_error("integer skill should not require nearest");
 inputs.hit_type=0;bool round_rejected=false;try{(void)vertical_combat_skill_success(actor,mob,0,inputs,random);}catch(const std::logic_error&){round_rejected=true;}
 if(std::fesetenv(&env))throw std::runtime_error("cannot restore fenv");if(!round_rejected||random.calls!=1)throw std::runtime_error("fallback rounding rejection");
 std::cout<<count<<" original skill success comparisons passed\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
