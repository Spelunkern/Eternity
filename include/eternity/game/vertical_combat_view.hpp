#pragma once
#include "eternity/game/vertical_combat_weapon_wear.hpp"
#include "eternity/game/user_priority_area.hpp"
namespace eternity::game {
// CZone::PSendView 00421220 and PSendViewCombat 004bd8d0 use radius 60,
// source actor ID as an immediate recipient, extra ID 0, priority 5.
// Borrow one actor, actual zone and its live buffer/connection/queue services.
// The service's origin() reads the retained actor's live position even during
// reentry; actor, zone and recipients remain retained and serialized.
class VerticalCombatView {
public:
 VerticalCombatView(Actor&,ZoneUsers&,std::int32_t width,std::int32_t height,UserPriorityAreaServices&);
 void send(Actor&,const net::Packet&);
 void send(Actor&,const net::Packet&,std::uint32_t immediate_target);
private:
 Actor& actor_;ZoneUsers& zone_;std::int32_t width_,height_;UserPriorityAreaServices& services_;
};
class VerticalCombatViewNormalWorld:public VerticalCombatWearNormalWorld {
public:
 VerticalCombatViewNormalWorld(RandomSource& random,ZoneUsers& users,VerticalCombatRangedPassive& passive,std::span<const float,81> elements,MobTargetingZone& targeting,const ItemDefinitions& items,VerticalCombatView& view)
  :VerticalCombatWearNormalWorld(random,users,passive,elements,targeting,items),view_(view){}
 void send_attack_view(Actor&,const net::Packet&) final;
private:VerticalCombatView& view_;
};
class VerticalCombatViewSkillServices:public VerticalCombatWearSkillServices {
public:
 VerticalCombatViewSkillServices(RandomSource& random,VerticalCombatSkillRuntimeDamage& damage,MobTargetingZone& targeting,const ItemDefinitions& items,VerticalCombatView& view)
  :VerticalCombatWearSkillServices(random,damage,targeting,items),view_(view){}
 void send_skill_view(Actor&,const net::Packet&) final;
 void send_secondary_combat(Actor&,const net::Packet&) final;
private:VerticalCombatView& view_;
};
}
