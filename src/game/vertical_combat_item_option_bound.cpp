#include "eternity/game/vertical_combat_item_option_bound.hpp"
#include <stdexcept>
namespace eternity::game {
VerticalCombatItemOptionBoundWorld::VerticalCombatItemOptionBoundWorld(Actor& a,std::uint8_t& guard,VerticalCombatSkillHitSelectServices& skills):actor_(a),guard_(guard),skills_(skills){}
std::uint8_t& VerticalCombatItemOptionBoundWorld::active(Actor& actor){if(&actor!=&actor_)throw std::invalid_argument("Item option guard bound to another actor");return guard_;}
VerticalCombatSkillSelectWorld& VerticalCombatItemOptionBoundWorld::select_world(){return skills_;}
VerticalCombatSkillHitPrefixWorld& VerticalCombatItemOptionBoundWorld::hit_world(){return skills_;}
VerticalCombatItemOptionNormalWorld::VerticalCombatItemOptionNormalWorld(RandomSource& r,ZoneUsers& u,VerticalCombatRangedPassive& p,std::span<const float,81> e,MobTargetingZone& t,const ItemDefinitions& i,VerticalCombatView& v,VerticalCombatHitServices& h,VerticalCombatItemOptionWorld& options):VerticalCombatHitNormalWorld(r,u,p,e,t,i,v,h),options_(options){}
void VerticalCombatItemOptionNormalWorld::attack_item_option(Actor& actor,MobRuntime& mob){vertical_combat_item_option(actor,mob,options_);}
void VerticalCombatItemOptionSkillServices::attack_item_option(Actor& actor,MobRuntime& mob){vertical_combat_item_option(actor,mob,item_option_world());}
}
