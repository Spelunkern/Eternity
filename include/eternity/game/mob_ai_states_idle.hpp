#pragma once
#include "eternity/game/mobs.hpp"
namespace eternity::game {
// Explicit live inputs missing from the canonical template or owned by world /
// chase storage. The owner retains this projection across synchronous callbacks.
struct MobAiIdleProjection {
    std::uint8_t ai=0,move_speed=0,normal_step=0;
    std::uint32_t normal_time=0;
    bool obelisk=false,chase_path_present=false,zone_available=false;
};
class MobAiIdleServices {
public:
    virtual ~MobAiIdleServices()=default;
    virtual std::uint32_t tick()=0;
    virtual bool find_target(MobRuntime&)=0;
    virtual void dispatch(MobRuntime&,std::int32_t event,std::uint32_t argument)=0;
    virtual std::int32_t random()=0; // Original CRT rand, serialized executor.
    virtual bool enable_line(MobRuntime&,std::int32_t start_x,std::int32_t start_z,std::int32_t end_x,std::int32_t end_z)=0;
    virtual void move(MobRuntime&,UserPosition,std::int32_t mode)=0;
};
// ps_game.exe CMob::WhileIdle 004b52c0; real target/line/move owners required.
void update_mob_ai_idle(MobRuntime&,const MobAiIdleProjection&,MobAiIdleServices&);
}
