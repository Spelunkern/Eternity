#include "eternity/game/vertical_combat_eligibility.hpp"
#include "eternity/game/ground_items.hpp"
#include <algorithm>
#include <stdexcept>
namespace eternity::game {
const GroundItem* vertical_combat_equipped(Actor& actor,std::uint16_t slot){
 if(slot!=5&&slot!=6)throw std::invalid_argument("Skill equipment requires weapon or shield slot");
 const auto held=actor.retained_inventory_item(0,slot);
 if(!held){
  if(!actor.inventory_slot_empty(0,slot))throw std::logic_error("Skill equipment requires retained item ownership");
  return nullptr;
 }
 if(!held->item||!held->pool||held->item->item.bag!=0||held->item->item.slot!=slot)throw std::logic_error("Retained skill equipment ownership mismatch");
 const auto& inventory=actor.inventory();
 if(std::count_if(inventory.begin(),inventory.end(),[slot](const PlayerItem& item){return item.bag==0&&item.slot==slot;})!=1)throw std::logic_error("Retained skill equipment mirror mismatch");
 return held->item;
}
const ItemDefinition& VerticalCombatEquippedNormalWorld::weapon(Actor& actor){
 const auto* item=vertical_combat_equipped(actor,5);
 if(!item||!item->definition)throw std::logic_error("Normal attack requires retained equipped weapon and definition");
 return *item->definition;
}
VerticalCombatEligibilityServices::VerticalCombatEligibilityServices(UserSkillRuntime& owner,VerticalCombatViewSkillServices& actions):owner_(owner),actions_(actions){
 if(!actions.bound_to_runtime(owner))throw std::invalid_argument("Skill eligibility and actions require the same runtime");
}
void VerticalCombatEligibilityServices::require_actor(const Actor& actor)const{if(&actor!=&owner_.actor())throw std::invalid_argument("Skill eligibility bound to another Actor");}
const GroundItem* VerticalCombatEligibilityServices::equipped(Actor& actor,std::uint16_t slot){require_actor(actor);return vertical_combat_equipped(actor,slot);}
void VerticalCombatEligibilityServices::user_effect(UserSkillAction action,Actor& actor,Actor* target,const UserSkillDefinition& info){require_actor(actor);actions_.user_effect(action,actor,target,info);}
void VerticalCombatEligibilityServices::mob_effect(UserSkillAction action,Actor& actor,MobRuntime* target,const UserSkillDefinition& info){require_actor(actor);actions_.mob_effect(action,actor,target,info);}
void VerticalCombatEligibilityServices::spend(Actor& actor,std::uint16_t sp,std::uint16_t mp){require_actor(actor);actions_.spend(actor,sp,mp);}
std::uint32_t VerticalCombatEligibilityServices::tick_ms(){return actions_.tick_ms();}
}
