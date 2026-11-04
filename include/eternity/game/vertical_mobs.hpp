#pragma once
#include "eternity/game/mob_primary_zones.hpp"
#include "eternity/game/mob_ai_states.hpp"
#include "eternity/game/mob_ai_states_configured.hpp"

namespace eternity::game {
// Required live global-list traversal, under the primary zone's serialized lease.
// It must preserve the original shared cursor across nested operations. The
// callback may remove actors; a snapshot of ZoneMobs::all() is not equivalent.
class VerticalMobsWorld {
public:
    virtual ~VerticalMobsWorld() = default;
    virtual void visit_mobs(ZoneMobs&,const std::function<void(MobRuntime&)>&) = 0;
    virtual MobAiConfiguredFsm& configured_fsm(MobRuntime&) = 0;
    virtual MobAiConfiguredServices& configured_services(MobRuntime&) = 0;
    virtual MobAiStateServices& stack_services(MobRuntime&) = 0;
    virtual void release_failed_stack(MobRuntime&) = 0; // ReleaseFSM and original error log
    virtual void return_home(MobRuntime&,std::uint32_t) = 0;
    // Remaining ordinary-idle targeting/movement body of CMob::UpdateIdle.
    virtual void ordinary_idle(MobRuntime&,std::uint32_t) = 0;
};
// ps_game.exe CMob::UpdateIdle 004a7e60: configured AI, Lua stack and
// ordinary idle routing. Services must reference the subject's retained owners.
void vertical_mobs_idle(MobRuntime&,std::uint32_t,VerticalMobsWorld&);
// CZone::UpdateMobIdle 00421a00: live traversal and death portion. The root
// must immediately follow this with its sole UpdateMobRespawn pass for this
// same zone and tick. Use the startup executor random source for corpse cleanup.
// No scheduling or attack pass is implied. False means no live primary lease.
bool vertical_mobs_idle_frame(MobPrimaryZones&,std::size_t,std::uint32_t,
    RandomSource&,VerticalMobsWorld&);
}
