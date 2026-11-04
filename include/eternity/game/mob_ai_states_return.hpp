#pragma once
#include "eternity/game/mobs.hpp"
namespace eternity::game {
struct MobAiReturnDefinition {std::uint8_t chase_step=0,size=0;std::uint32_t chase_time=0;};
class MobAiReturnServices {
public:
    virtual ~MobAiReturnServices()=default;
    virtual std::uint32_t tick()=0;
    // Real ResetAttack owner (including its attack/skill definition bindings).
    virtual void reset_attack(MobRuntime&)=0;
    virtual void dispatch(MobRuntime&,std::int32_t event,std::uint32_t argument)=0;
    virtual bool move(MobRuntime&,UserPosition,std::int32_t mode)=0;
};
// CMob::WhileReturnHome 004b5c40. Actual native map, A*, trail and damage
// owners are composed here. Subject/definition/map must survive callbacks.
void update_mob_ai_return_home(MobRuntime&,const MobAiReturnDefinition&,const MapDefinition&,MobAiReturnServices&);
}
