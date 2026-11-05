#include "eternity/game/vertical_combat_skill_primary.hpp"
namespace eternity::game {
namespace {
void word(net::Packet& p,std::uint16_t v){p.push_back(static_cast<std::uint8_t>(v));p.push_back(static_cast<std::uint8_t>(v>>8));}
void dword(net::Packet& p,std::uint32_t v){word(p,static_cast<std::uint16_t>(v));word(p,static_cast<std::uint16_t>(v>>16));}
}
void vertical_combat_skill_primary(Actor& actor,MobRuntime& mob,const UserSkillDefinition& info,bool first,bool second,bool critical,std::array<std::int32_t,3>& damage,VerticalCombatSkillPrimaryWorld& world){
 const bool drain=first&&info.effect.detail==27;
 const bool killed=world.apply_primary(actor,mob,info,damage);
 const auto hp=damage[0];
 std::array<std::int32_t,3> healed{};
 if(drain)healed=actor.heal(damage);
 net::Packet p;word(p,drain?0x518:first?0x517:0x519);p.push_back(critical?1:0);dword(p,actor.id());dword(p,mob.id);word(p,info.effect.id);p.push_back(info.effect.level);
 word(p,static_cast<std::uint16_t>(hp));word(p,static_cast<std::uint16_t>(damage[1]));word(p,static_cast<std::uint16_t>(damage[2]));
 if(drain)for(auto value:healed)word(p,static_cast<std::uint16_t>(value));
 world.send_primary_view(actor,p);
 world.after_primary(actor,mob,info,first,second,killed,drain,damage);
}
void VerticalCombatSkillPrimaryServices::finish_skill_hit(Actor& actor,MobRuntime& mob,const UserSkillDefinition& info,bool first,bool second,bool critical,std::array<std::int32_t,3>& damage){
 vertical_combat_skill_primary(actor,mob,info,first,second,critical,damage,*this);
}
void VerticalCombatSkillPrimaryServices::send_primary_view(Actor& actor,const net::Packet& packet){send_skill_view(actor,packet);}
}
