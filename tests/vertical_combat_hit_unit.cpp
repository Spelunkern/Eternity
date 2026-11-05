#include "eternity/game/vertical_combat_hit.hpp"
#include "eternity/game/item_pool.hpp"
#include "equipment_fixture.hpp"
#include <functional>
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
struct Pool:MobDamagePoolServices {std::function<void()> callback;std::unique_ptr<MobDamageRecord> construct()override{if(callback)callback();return std::make_unique<MobDamageRecord>();}};
struct Hits:VerticalCombatHitServices {
 std::optional<std::uint32_t> party;std::int32_t bonus=0;bool killed=false;unsigned calls=0,deaths=0;MobCombatCredit captured;std::array<std::int32_t,3> captured_damage{};
 std::optional<std::uint32_t> party_id(const Actor&)override{return party;}
 std::int32_t aggro_bonus(const Actor&)override{return bonus;}
 bool damage(Actor&,MobRuntime&,std::array<std::int32_t,3>& damage,const MobCombatCredit& credit)override{++calls;captured=credit;captured_damage=damage;return killed;}
 void death(MobRuntime&,std::int32_t type,std::uint32_t id)override{if(type!=1||id!=42)throw std::runtime_error("death args");++deaths;}
};
int main(){try{
 auto items=ItemDefinitions::load(item_definitions());EnchantValues enchant;MobAttackTemplates templates;ActorEquipmentRules rules{items,enchant,templates};Scene scene;Actor actor(42,{}, {},{},scene,rules);
 Pool allocation;MobDamagePool pool(allocation);MobRuntime mob;mob.damage=std::make_unique<MobDamageLedger>(pool);mob.hp=100;mob.status=1;
 Hits hits;hits.party=5;hits.bonus=3;SkillDefinition effect;effect.aggro=6;UserSkillDefinition skill{effect,"test"};std::array<std::int32_t,3> primary{13,1,2},secondary{7,3,4};
 allocation.callback=[&]{hits.party=9;hits.bonus=8;effect.aggro=17;};
 if(vertical_combat_hit(actor,mob,&skill,primary,secondary,hits)||hits.calls!=1||hits.captured.party!=9||hits.captured.bonus!=8||hits.captured.base_aggro!=17||hits.captured_damage!=secondary)throw std::runtime_error("live party reload");
 auto records=mob.damage->records();if(records.size()!=1||records[0]->type!=5||records[0]->id!=5||records[0]->damage!=13)throw std::runtime_error("secondary party credit");
 allocation.callback={};hits.party.reset();hits.killed=true;actor.attack_state().mode=2;
 if(!vertical_combat_hit(actor,mob,nullptr,primary,primary,hits)||hits.deaths!=1||hits.captured.party!=0||hits.captured.base_aggro!=0||actor.attack_state().mode!=0)throw std::runtime_error("normal lethal sequence");
 hits.party=10;hits.killed=false;mob.hp=0;const auto before=mob.damage->records().size();vertical_combat_hit(actor,mob,&skill,primary,primary,hits);if(mob.damage->records().size()!=before)throw std::runtime_error("dead mob party credit");
 mob.hp=100;hits.party=11;allocation.callback=[&]{hits.party.reset();};bool rejected=false;try{vertical_combat_hit(actor,mob,&skill,primary,primary,hits);}catch(const std::logic_error&){rejected=true;}if(!rejected)throw std::runtime_error("lost party accepted");
 MobRuntime unbound;hits.party.reset();rejected=false;try{vertical_combat_hit(actor,unbound,nullptr,primary,primary,hits);}catch(const std::logic_error&){rejected=true;}if(!rejected)throw std::runtime_error("missing ledger accepted");
 std::cout<<"live party credit and mandatory hit sequence passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
