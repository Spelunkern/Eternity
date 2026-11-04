#include "eternity/game/vertical_combat.hpp"
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
struct World:VerticalCombatWorld {
 Actor& actor;MobRuntime& mob;ItemDefinition item;bool success,critical,killed,zone;std::string events;
 World(Actor& a,MobRuntime& m):actor(a),mob(m){}
 void add(std::string s){if(!events.empty())events+='/';events+=s;}
 void check(Actor& a,MobRuntime& m){if(&a!=&actor||&m!=&mob)throw std::runtime_error("wrong owner");}
 void remove_sleep(MobRuntime& m) override {if(&m!=&mob)throw std::runtime_error("wrong mob");add("S");}
 const ItemDefinition& weapon(Actor& a) override {if(&a!=&actor)throw std::runtime_error("wrong actor");return item;}
 bool attack_success(Actor& a,MobRuntime& m,unsigned k) override {check(a,m);add("H"+std::to_string(k));return success;}
 bool attack_damage(Actor& a,MobRuntime& m,unsigned k,std::array<std::int32_t,3>& d) override {check(a,m);add("C"+std::to_string(k));d={0x12345,0x23456,0x34567};return critical;}
 void wear_weapon(Actor&) override {add("W");}
 bool apply_hit(Actor& a,MobRuntime& m,std::array<std::int32_t,3>& d) override {check(a,m);if(d[0]!=0x12345)throw std::runtime_error("damage");add("D");if(killed){add("X");add("K");}return killed;}
 void send_attack_view(Actor&,const net::Packet& p) override {if(zone){const char* h="0123456789abcdef";std::string v="P";for(auto b:p){v+=h[b>>4];v+=h[b&15];}add(v);}}
 void set_target(MobRuntime& m,Actor& a) override {check(a,m);add("T");}
 void attack_item_option(Actor& a,MobRuntime& m) override {check(a,m);if(!m.attacked||!m.in_battle||!m.attack_at_moment)throw std::runtime_error("flag order");add("I");}
};
}
int main(int argc,char**argv){try{
 if(argc!=2)throw std::runtime_error("fixture required");std::ifstream input(argv[1]);if(!input)throw std::runtime_error("fixture missing");
 ItemDefinitions items;EnchantValues enchant;MobAttackTemplates templates;ActorEquipmentRules rules{items,enchant,templates};std::string line;unsigned count=0;
 while(std::getline(input,line)){if(line.empty()||line[0]=='#')continue;unsigned type,blocked,success,critical,killed,dead,zone;std::string expected,flags;std::istringstream row(line);row>>type>>blocked>>success>>critical>>killed>>dead>>zone>>expected>>flags;if(!row)throw std::runtime_error("bad row");
 Scene scene;EffectStatus status;const auto kind=(type==11||type==13||type==14)?1:0;status.skills.attack_disabled[kind]=blocked;Actor actor(42,status,{}, {},scene,rules);actor.attack_state().mode=dead?1:0;MobRuntime mob;mob.id=77;World w(actor,mob);w.item.real_type=type;w.success=success;w.critical=critical;w.killed=killed;w.zone=zone;
 vertical_combat_attack_mob(actor,mob,w);if(expected=="-")expected.clear();const std::string actual_flags=mob.attacked&&mob.in_battle&&mob.attack_at_moment?"010101":"000000";if(w.events!=expected||flags!=actual_flags)throw std::runtime_error("case "+std::to_string(count)+" actual "+w.events+" expected "+expected);++count;}
 if(count!=256)throw std::runtime_error("incomplete fixture");std::cout<<count<<" original caller traces passed\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
