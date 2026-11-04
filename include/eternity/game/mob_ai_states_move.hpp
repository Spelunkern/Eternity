#pragma once
#include "eternity/game/mob_events.hpp"
namespace eternity::game {
struct MobAiMoveDefinition {std::uint8_t normal_step=0,size=0;std::uint32_t normal_time=0;};
class MobAiMoveServices:public MobEventServices {
public:
    virtual bool find_target(MobRuntime&)=0;
    virtual std::int32_t random()=0;
    virtual void reset_attack(MobRuntime&)=0;
    virtual void dispatch(MobRuntime&,std::int32_t event,std::uint32_t argument)=0;
    virtual bool move(MobRuntime&,UserPosition,std::int32_t mode)=0;
};
// CMob::WhileMove 004b5790. Required real target/Lua/FSM/zone owners; composes
// the actual native map, path, trail and damage storage, using live definitions.
void update_mob_ai_move(MobRuntime&,const MobAiMoveDefinition&,const MapDefinition&,MobAiMoveServices&);
}
