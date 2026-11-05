#include "eternity/game/vertical_combat_skill_hit_prefix.hpp"
#include <stdexcept>
namespace eternity::game {
namespace {
void word(net::Packet& p,std::uint16_t v){p.push_back(v&255);p.push_back(v>>8);}
void dword(net::Packet& p,std::uint32_t v){word(p,v&65535);word(p,v>>16);}
}
void vertical_combat_skill_hit_prefix(Actor& actor,MobRuntime& mob,const UserSkillDefinition& info,bool first,bool second,VerticalCombatSkillHitPrefixWorld& world){
 if(world.skill_success(actor,mob,info)){
  if(first){
   world.remove_sleep(mob);
   if(info.effect.id==56)actor.movement_check().maximum_speed-=static_cast<float>(info.attack_range);
  }
  world.continue_hit(actor,mob,info,first,second);
  return;
 }
 const bool drain=first&&info.effect.detail==27;
 net::Packet p;word(p,first?(drain?0x518:0x517):0x519);p.push_back(3);dword(p,actor.id());dword(p,mob.id);word(p,info.effect.id);p.push_back(info.effect.level);
 p.resize(drain?26:20,0);
 world.send_skill_view(actor,p);
 if(!drain){
  world.set_target(mob,actor);
  if(mob.battle_type==0){mob.attack_at_moment=true;mob.attacked=true;mob.in_battle=true;}
 }
 if(first&&info.effect.id==56)actor.movement_check().maximum_speed-=static_cast<float>(info.attack_range);
}
bool VerticalCombatSkillHitSuccessWorld::skill_success(Actor& actor,MobRuntime& mob,const UserSkillDefinition& info){
 const auto input=success_inputs(mob,info);
 if(&input.skill!=&info)throw std::invalid_argument("Skill success input must retain captured definition");
 return vertical_combat_skill_success(actor,mob,info.attack_kind,input,random_);
}
bool VerticalCombatSkillHitSelectSuccessServices::skill_success(Actor& actor,MobRuntime& mob,const UserSkillDefinition& info){
 const auto input=success_inputs(mob,info);
 if(&input.skill!=&info)throw std::invalid_argument("Skill success input must retain captured definition");
 return vertical_combat_skill_success(actor,mob,info.attack_kind,input,random_);
}
void VerticalCombatSkillHitSelectServices::other_mob_effect(UserSkillAction action,Actor& actor,MobRuntime* mob,const UserSkillDefinition& info){
 if(action!=UserSkillAction::debuff_select){remaining_mob_effect(action,actor,mob,info);return;}
 if(!mob)throw std::invalid_argument("Selected mob debuff requires retained target");
 hit(actor,*mob,info,true,true);
}
void VerticalCombatSkillHitSelectServices::hit(Actor& actor,MobRuntime& mob,const UserSkillDefinition& info,bool first,bool second){
 vertical_combat_skill_hit_prefix(actor,mob,info,first,second,*this);
}
}
