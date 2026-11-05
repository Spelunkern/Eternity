#include "eternity/game/vertical_combat_skill_tail.hpp"
namespace eternity::game {
namespace {
void word(net::Packet& p,std::uint16_t v){p.push_back(static_cast<std::uint8_t>(v));p.push_back(static_cast<std::uint8_t>(v>>8));}
void dword(net::Packet& p,std::uint32_t v){word(p,static_cast<std::uint16_t>(v));word(p,static_cast<std::uint16_t>(v>>16));}
}
void vertical_combat_skill_tail(Actor& actor,MobRuntime& mob,const UserSkillDefinition& info,bool first,bool second,bool killed,bool drain,std::array<std::int32_t,3>& primary,VerticalCombatSkillTailWorld& world){
 if(!drain){
  const auto words=world.secondary_words(info);
  if((words[0]||words[1]||words[2])&&!killed){
   std::array<std::int32_t,3> damage{words[0],words[1],words[2]};
   const auto hp=damage[0];
   killed=world.apply_secondary(actor,mob,info,primary,damage);
   net::Packet p;word(p,0x51a);dword(p,actor.id());dword(p,mob.id);word(p,info.effect.id);p.push_back(info.effect.level);
   word(p,static_cast<std::uint16_t>(hp));word(p,static_cast<std::uint16_t>(damage[1]));word(p,static_cast<std::uint16_t>(damage[2]));
   world.send_secondary(actor,first,p);
  }
 }
 world.set_target(mob,actor);
 mob.attack_at_moment=true;mob.attacked=true;mob.in_battle=true;
 if(!killed&&info.effect.effect==3)world.add_mob_skill(mob,actor,info);
 if(second)world.attack_item_option(actor,mob);
}
void VerticalCombatSkillTailServices::after_primary(Actor& actor,MobRuntime& mob,const UserSkillDefinition& info,bool first,bool second,bool killed,bool drain,std::array<std::int32_t,3>& primary){
 vertical_combat_skill_tail(actor,mob,info,first,second,killed,drain,primary,*this);
}
void VerticalCombatSkillTailServices::send_secondary(Actor& actor,bool first,const net::Packet& packet){
 if(first)send_secondary_combat(actor,packet);else send_skill_view(actor,packet);
}
}
