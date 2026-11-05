#include "eternity/game/vertical_combat_view.hpp"
#include <stdexcept>
namespace eternity::game {
VerticalCombatView::VerticalCombatView(Actor& actor,ZoneUsers& zone,std::int32_t width,std::int32_t height,UserPriorityAreaServices& services)
 :actor_(actor),zone_(zone),width_(width),height_(height),services_(services){
 if(width<=0||height<=0)throw std::invalid_argument("Combat view requires valid zone dimensions");
}
void VerticalCombatView::send(Actor& actor,const net::Packet& packet){send(actor,packet,0);}
void VerticalCombatView::send(Actor& actor,const net::Packet& packet,std::uint32_t immediate_target){
 if(&actor!=&actor_)throw std::invalid_argument("Combat view bound to another Actor");
 const auto* current=zone_.find_actor(actor.id());
 if(current&&current!=&actor)throw std::logic_error("Combat view actor identity changed");
 if(!current){if(actor.status().in_zone)throw std::logic_error("Combat view actor belongs to another zone");return;}
 user_priority_area(zone_,width_,height_,packet,0.0f,60.0f,false,actor.id(),immediate_target,5,services_);
}
void VerticalCombatViewNormalWorld::send_attack_view(Actor& actor,const net::Packet& packet){view_.send(actor,packet);}
void VerticalCombatViewSkillServices::send_skill_view(Actor& actor,const net::Packet& packet){view_.send(actor,packet);}
void VerticalCombatViewSkillServices::send_secondary_combat(Actor& actor,const net::Packet& packet){view_.send(actor,packet);}
}
