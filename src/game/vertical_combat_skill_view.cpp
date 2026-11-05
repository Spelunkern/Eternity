#include "eternity/game/vertical_combat_skill_view.hpp"
namespace eternity::game {
void vertical_combat_announce_mob(Actor& actor,MobRuntime& mob,const UserSkillDefinition& skill,VerticalCombatView& view){
 net::Packet packet(20,0);const auto put=[&](unsigned at,std::uint32_t value,unsigned size){for(unsigned i=0;i<size;++i)packet[at+i]=static_cast<std::uint8_t>(value>>(8*i));};
 put(0,0x517,2);packet[2]=4;put(3,actor.id(),4);put(7,mob.id,4);put(11,skill.effect.id,2);packet[13]=skill.effect.level;view.send(actor,packet);
}
void vertical_combat_announce_user(Actor& actor,Actor& target,const UserSkillDefinition& skill,VerticalCombatView& view){
 net::Packet packet(20,0);const auto put=[&](unsigned at,std::uint32_t value,unsigned size){for(unsigned i=0;i<size;++i)packet[at+i]=static_cast<std::uint8_t>(value>>(8*i));};
 put(0,0x511,2);packet[2]=4;put(3,actor.id(),4);put(7,target.id(),4);put(11,skill.effect.id,2);packet[13]=skill.effect.level;view.send(actor,packet,target.id());
}
VerticalCombatSkillViewServices::VerticalCombatSkillViewServices(RandomSource& r,VerticalCombatSkillRuntimeDamage& d,MobTargetingZone& z,const ItemDefinitions& i,VerticalCombatView& v,VerticalCombatHitServices& h,VerticalCombatSkillSlotServices& s,UserSkillSpendGroupWorld& g,const VerticalCombatUserSkillRecordSource& u,const MobRuntimeTemplates& t,const MobTargetingBindingCatalog& m):VerticalCombatUserSkillRecordsServices(r,d,z,i,v,h,s,g,u,t,m),view_(v){}
void VerticalCombatSkillViewServices::announce_multi(Actor& a,MobRuntime& m,const UserSkillDefinition& s){vertical_combat_announce_mob(a,m,s,view_);}
VerticalCombatMobCastEligibilityServices::VerticalCombatMobCastEligibilityServices(UserSkillRuntime& r,VerticalCombatViewSkillServices& s,VerticalCombatRange& range,VerticalCombatView& view):VerticalCombatRangeEligibilityServices(r,s,range),view_(view){}
void VerticalCombatMobCastEligibilityServices::cast_mob(Actor& actor,MobRuntime&,const UserSkillDefinition&,const net::Packet& packet){view_.send(actor,packet);}
VerticalCombatCastEligibilityServices::VerticalCombatCastEligibilityServices(UserSkillRuntime& r,VerticalCombatViewSkillServices& s,VerticalCombatRange& range,VerticalCombatView& view):VerticalCombatMobCastEligibilityServices(r,s,range,view),user_view_(view){}
void VerticalCombatCastEligibilityServices::cast_user(Actor& actor,Actor& target,const UserSkillDefinition&,const net::Packet& packet){user_view_.send(actor,packet,target.id());}
}
