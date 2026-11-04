#pragma once
#include "eternity/game/mob_events.hpp"
#include <span>
namespace eternity::game {
enum class MobAiState : std::int32_t { idle=0, combat=1, death=2, return_home=3, move=4 };
enum class MobAiOperation { push=0, pop=1, replace=2 };
struct MobAiTransition {
    std::int32_t from,event,to;
    MobAiOperation operation;
    MobEvent action;
};
std::span<const MobAiTransition> mob_ai_transitions();
enum class MobAiError { empty_dispatch, unhandled_event, overflow, empty_pop, empty_poll, invalid_initial };
class MobAiStateServices : public MobEventServices {
public:
    // Implement the actual WhileIdle/Combat/ReturnHome/Move routines. Death is
    // implemented here: original WhileDeath 004b5780 is empty.
    virtual void update(MobRuntime&,MobAiState) = 0;
    virtual void error(MobRuntime&,MobAiError,std::int32_t state,std::int32_t event) = 0;
};
// Compiled stack table from CMob::InitLuaFSM 004b4c20; uses the existing
// state_stack projection. Lua allocation/registration remains a separate owner.
// Initial selection clears the stack without leave callbacks. Status is retained.
void initialize_mob_ai_stack(MobRuntime&,std::int32_t initial,MobAiStateServices&);
void dispatch_mob_ai_event(MobRuntime&,std::int32_t event,std::uint32_t argument,MobAiStateServices&);
bool poll_mob_ai_stack(MobRuntime&,MobAiStateServices&);
}
