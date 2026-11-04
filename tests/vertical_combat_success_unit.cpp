#include "eternity/game/vertical_combat_success.hpp"
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
 std::cout<<count<<" original success comparisons passed\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
