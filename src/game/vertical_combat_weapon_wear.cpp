#include "eternity/game/vertical_combat_weapon_wear.hpp"
#include "eternity/game/ground_items.hpp"
#include <algorithm>
#include <stdexcept>
namespace eternity::game {
void vertical_combat_wear_weapon(Actor& actor,const ItemDefinitions& definitions){
 auto& status=actor.status();
 if(status.skills.benefits.no_durability_loss!=0||status.shape!=0)return;
 auto& inventory=actor.inventory();
 const auto equipped=[](const PlayerItem& item){return item.bag==0&&item.slot==5;};
 const auto mirror=std::find_if(inventory.begin(),inventory.end(),equipped);
 if(mirror!=inventory.end()&&std::find_if(mirror+1,inventory.end(),equipped)!=inventory.end())throw std::invalid_argument("Duplicate equipped weapons");
 const auto held=actor.retained_inventory_item(0,5);
 if(!held){if(mirror!=inventory.end())throw std::logic_error("Weapon wear requires retained item ownership");return;}
 if(!held->item||mirror==inventory.end())throw std::logic_error("Retained weapon inventory mismatch");
 auto& item=*held->item;
 if(item.item.bag!=0||item.item.slot!=5)throw std::logic_error("Retained weapon slot mismatch");
 if(item.item.quality==0)return;
 const auto* definition=definitions.find(item.item.type,item.item.type_id);
 if(!definition||item.definition!=definition)throw std::invalid_argument("Weapon wear requires its retained definition owner");
 const auto result=decrease_equipment_quality(status.skills.equipment,5,&item.item,1,definitions);
 if(!actor.sync_inventory_item(0,5,&item))throw std::logic_error("Retained weapon lost during quality update");
 if(result.attack_changed)recalculate_attack(status);
}
void VerticalCombatWearNormalWorld::wear_weapon(Actor& actor){vertical_combat_wear_weapon(actor,items_);}
void VerticalCombatWearSkillServices::wear_weapon(Actor& actor){vertical_combat_wear_weapon(actor,items_);}
}
