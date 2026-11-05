#include "eternity/game/vertical_combat_weapon_wear.hpp"
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
int main(int argc,char**argv){try{
 if(argc!=2)throw std::runtime_error("fixture required");std::ifstream input(argv[1]);if(!input)throw std::runtime_error("missing fixture");
 auto items=ItemDefinitions::load(item_definitions());EnchantValues enchant;MobAttackTemplates templates;ActorEquipmentRules rules{items,enchant,templates};std::string line;unsigned count=0;
 while(std::getline(input,line)){
  if(line.empty()||line[0]=='#')continue;unsigned protection,shape,present,quality,cached,final_quality,final_cached;std::string events;std::istringstream row(line);
  row>>protection>>shape>>present>>quality>>cached>>events>>final_quality>>final_cached;if(!row)throw std::runtime_error("bad fixture");
  Scene scene;GroundItemPool pool;EffectStatus state;state.shape=shape;state.skills.benefits.no_durability_loss=protection;state.skills.equipment.quality[5]=cached;state.skills.equipment.attack.item_attack=2000;state.skills.equipment.variance=2000;state.calculated_attack[0].attack=43210;
  Actor actor(42,state,{}, {},scene,rules);GroundItem* item=nullptr;
  if(present){item=pool.allocate();item->definition=items.find(5,1);item->item.type=5;item->item.type_id=1;item->item.quality=quality;item->item.count=1;if(!actor.claim_inventory_item(0,5,*item,pool))throw std::runtime_error("claim failed");}
  vertical_combat_wear_weapon(actor,items);
  if((item?item->item.quality:quality)!=final_quality||actor.status().skills.equipment.quality[5]!=final_cached)throw std::runtime_error("quality mismatch case "+std::to_string(count));
  if(item&&(actor.inventory().size()!=1||actor.inventory()[0].quality!=item->item.quality))throw std::runtime_error("pooled/mirror mismatch");
  const bool rebuilt=events.find('T')!=std::string::npos;
  if(rebuilt){auto expected=calculate_attack(actor.status().level,actor.status().skills.equipment.base.attributes,actor.status().skills.equipment.attack);if(actor.status().calculated_attack[0].attack!=expected[0].attack)throw std::runtime_error("attack rebuild missing");}
  else if(actor.status().calculated_attack[0].attack!=43210)throw std::runtime_error("unexpected attack rebuild");
  ++count;
 }
 if(count!=168)throw std::runtime_error("incomplete fixture");
 Scene scene;PlayerItem mirror;mirror.bag=0;mirror.slot=5;mirror.type=5;mirror.type_id=1;mirror.quality=1000;Actor projected(42,{}, {},{},scene,rules,std::vector<PlayerItem>{mirror});
 bool rejected=false;try{vertical_combat_wear_weapon(projected,items);}catch(const std::logic_error&){rejected=true;}if(!rejected||projected.inventory()[0].quality!=1000)throw std::runtime_error("unretained weapon accepted");
 projected.status().shape=1;vertical_combat_wear_weapon(projected,items);projected.status().shape=0;projected.status().skills.benefits.no_durability_loss=1;vertical_combat_wear_weapon(projected,items);
 GroundItemPool pool;auto other=ItemDefinitions::load(item_definitions());Actor actor(43,{}, {},{},scene,rules);auto* weapon=pool.allocate();weapon->definition=other.find(5,1);weapon->item.type=5;weapon->item.type_id=1;weapon->item.quality=501;weapon->item.count=1;if(!actor.claim_inventory_item(0,5,*weapon,pool))throw std::runtime_error("foreign setup");
 rejected=false;try{vertical_combat_wear_weapon(actor,items);}catch(const std::invalid_argument&){rejected=true;}if(!rejected||weapon->item.quality!=501||actor.inventory()[0].quality!=501)throw std::runtime_error("foreign definition accepted");
 std::cout<<count<<" original weapon wear comparisons and retained-owner guards passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
