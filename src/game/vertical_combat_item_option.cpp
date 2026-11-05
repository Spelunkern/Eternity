#include "eternity/game/vertical_combat_item_option.hpp"
#include "eternity/game/vertical_combat_scheduler_target.hpp"
#include "eternity/game/ground_items.hpp"
#include <algorithm>
#include <stdexcept>
namespace eternity::game {
void vertical_combat_item_option(Actor& actor,MobRuntime& mob,VerticalCombatItemOptionWorld& world){
 auto& active=world.active(actor);
 if(active || mob_status(mob)==2 || actor.attack_state().mode==1)return;
 if(!vertical_combat_equipped(actor,5))return;
 active=1;struct Guard{std::uint8_t& active;~Guard(){active=0;}} guard{active};
 for(std::size_t i=0;i<6;++i){
  const auto* weapon=vertical_combat_equipped(actor,5);if(!weapon)throw std::logic_error("Item option weapon disappeared during callback");
  const auto id=weapon->item.gems[i];if(!id)continue;
  const auto item=world.jewel(id);if(item.empty())continue;
  if(item.size()<0x47)throw std::logic_error("Item option requires complete jewel definition");
  if(item[0x24]==std::byte{0} || item[0x25]==std::byte{0})continue;
  const auto skill_id=std::to_integer<std::uint8_t>(item[0x46]);if(!skill_id || skill_id==16)continue;
  const auto* skill=world.mob_skill(skill_id);if(!skill)continue;
  const bool naming=vertical_combat_is_both_mob(mob);
  auto& name=mob.previous_attack_skill;
  const auto end=std::find(name.begin(),name.end(),0);
  if(naming && end==name.end())throw std::logic_error("Item option prior skill name exceeds buffer");
  std::array<std::uint8_t,32> saved{};std::size_t length=0;
  if(naming){length=static_cast<std::size_t>(end-name.begin())+1;std::copy_n(name.begin(),length,saved.begin());const auto n=std::min(skill->name.size(),skill->name.find('\0'));if(n>=name.size())throw std::invalid_argument("Item option skill name exceeds buffer");std::copy_n(skill->name.begin(),n,name.begin());name[n]=0;}
  if(skill->effect.effect==1)vertical_combat_skill_select(actor,mob,*skill,world.select_world());
  else if(skill->effect.effect==3)vertical_combat_skill_hit_prefix(actor,mob,*skill,true,true,world.hit_world());
  if(naming)std::copy_n(saved.begin(),length,name.begin());
 }
}
}
