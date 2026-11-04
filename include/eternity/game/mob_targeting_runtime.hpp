#pragma once
#include "eternity/game/mob_targeting.hpp"
namespace eternity::game {
struct MobRuntime;
struct MobTargetingBindings {
    std::uint8_t ai=0;
    std::array<MobTargetingAttack,3> attacks{};
};
// Production boundary for the original targeting calls. Bindings must describe
// the live definition/skill links, not inferred attack damage or default skills.
struct MobTargetingRuntimeServices {
    virtual ~MobTargetingRuntimeServices()=default;
    virtual MobTargetingBindings bindings(const MobRuntime&) const=0;
    virtual std::int32_t random(MobRuntime&)=0;
    virtual std::uint32_t tick_ms(MobRuntime&)=0;
    virtual void help(MobRuntime&,const MobTargetingUser&,std::uint8_t kind)=0;
    virtual void dispatch(MobRuntime&,std::uint32_t event,std::uint32_t argument)=0;
};
// Scene stack lifetime/transitions belong to dispatch/help providers. The bridge
// reads the current top (empty active stack => 0) and never replaces that stack.
void mob_set_target(MobRuntime&,const MobTargetingUser&,bool initialize,MobTargetingRuntimeServices&);
void mob_set_target_ex(MobRuntime&,const MobTargetingUser&,MobTargetingRuntimeServices&);
}
