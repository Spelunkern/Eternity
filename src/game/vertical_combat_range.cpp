#include "eternity/game/vertical_combat_range.hpp"
#include "eternity/game/vertical_combat_success.hpp"
#include "eternity/game/vertical_movement.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace eternity::game {
VerticalCombatRange::VerticalCombatRange(Actor& a,ZoneUsers& z,VerticalMotionState& m,std::span<const float,5> f,VerticalCombatRangeServices& s):actor_(a),zone_(z),movement_(m),factors_(f),services_(s){}
UserPosition VerticalCombatRange::position(Actor& a)const{
 if(zone_.find_actor(a.id())!=&a)throw std::logic_error("Combat range requires retained same-zone actor");
 const auto p=zone_.position(a.id());if(!p)throw std::logic_error("Combat range requires live position");return *p;
}
float VerticalCombatRange::allowance(){
 vertical_combat_require_float_environment();
 const auto index=std::clamp(actor_.status().skills.equipment.move_speed,0,4);
 const float factor=factors_[index];
 if(!std::isfinite(factor))throw std::logic_error("Combat range factor must be finite");
 float value=static_cast<float>(static_cast<double>(factor)*3.0);
 if(!std::isfinite(value))throw std::logic_error("Combat range overflow");
 const auto now=services_.tick_ms();
 vertical_combat_require_float_environment();
 (void)vertical_movement_planar_distance({},{}); // also recheck SSE gradual underflow after callback
 const auto elapsed=now-movement_.last_tick;
 if(elapsed<=1000){const float ticks=static_cast<float>(elapsed);value=static_cast<float>((static_cast<double>(ticks)/1000.0)*value);}
 if(!std::isfinite(value))throw std::logic_error("Combat range overflow");
 return std::max(value,2.0f);
}
bool VerticalCombatRange::user(Actor& a,Actor& target,std::int32_t extra){
 if(&a!=&actor_)throw std::invalid_argument("Combat range requires bound actor");
 const auto destination=position(target),source=position(a);
 const float distance=vertical_movement_planar_distance(destination,source);
 const float allowed=allowance();
 const float weapon=static_cast<float>(a.status().skills.equipment.range),addition=static_cast<float>(extra);
 const double total=static_cast<double>(allowed)+weapon+addition+static_cast<double>(2.3f);
 return total>=distance;
}
bool VerticalCombatRange::mob(Actor& a,MobRuntime& target,std::int32_t extra){
 if(&a!=&actor_)throw std::invalid_argument("Combat range requires bound actor");
 const auto destination=services_.mob_current_position(target),source=position(a);
 const float distance=vertical_movement_planar_distance(destination,source);
 const float allowed=allowance();
 const float size=static_cast<float>(services_.mob_size(target));
 const float weapon=static_cast<float>(a.status().skills.equipment.range),addition=static_cast<float>(extra);
 const double total=static_cast<double>(allowed)+size+weapon+addition+static_cast<double>(2.3f);
 return total>=distance;
}
VerticalCombatRangeEligibilityServices::VerticalCombatRangeEligibilityServices(UserSkillRuntime& o,VerticalCombatViewSkillServices& a,VerticalCombatRange& r):VerticalCombatEligibilityServices(o,a),range_(r){}
bool VerticalCombatRangeEligibilityServices::in_user_range(Actor& a,Actor& t,std::uint8_t extra){return range_.user(a,t,extra);}
bool VerticalCombatRangeEligibilityServices::in_mob_range(Actor& a,MobRuntime& t,std::uint8_t extra){return range_.mob(a,t,extra);}
VerticalCombatRangeSchedulerWorld::VerticalCombatRangeSchedulerWorld(UserSkillRuntime& o,UserSkillUseServices& s,VerticalCombatWearNormalWorld& n,VerticalCombatRange& r):VerticalCombatSchedulerWorld(o,s,n),range_(r){}
bool VerticalCombatRangeSchedulerWorld::in_range(Actor& a,MobRuntime& t){return range_.mob(a,t,0);}
bool VerticalCombatRangeSchedulerWorld::in_user_range(Actor& a,Actor& t){return range_.user(a,t,0);}
}
