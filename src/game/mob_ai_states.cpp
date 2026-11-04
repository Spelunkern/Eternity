#include "eternity/game/mob_ai_states.hpp"
#include <array>
#include <stdexcept>
namespace eternity::game {
namespace {
using O=MobAiOperation; using E=MobEvent;
constexpr std::array<MobAiTransition,23> table{{
    MobAiTransition{0,0,1,O::push,E::attackable}, {0,1,1,O::push,E::attacked},
    {1,1,1,O::replace,E::attacked}, {0,5,4,O::push,E::move}, {0,7,4,O::push,E::move},
    {4,6,0,O::pop,E::move_end}, {4,5,4,O::replace,E::move}, {4,7,4,O::replace,E::move},
    {1,7,0,O::pop,E::move}, {1,5,4,O::replace,E::move}, {3,5,0,O::pop,E::move},
    {4,0,1,O::push,E::attackable}, {4,1,1,O::push,E::attacked},
    {0,4,3,O::push,E::return_home}, {1,4,3,O::replace,E::return_home},
    {0,2,0,O::replace,E::normal_reset}, {1,2,0,O::pop,E::normal_reset},
    {3,2,0,O::pop,E::normal_reset}, {4,2,0,O::pop,E::normal_reset},
    {1,3,2,O::replace,E::death}, {0,3,2,O::replace,E::death},
    {3,3,2,O::replace,E::death}, {4,3,2,O::replace,E::death}
}};
void validate(const MobRuntime& mob) {
    if (!mob.state_stack) throw std::logic_error("Mob stack FSM has not been initialized");
    for(auto state:*mob.state_stack) if(state<0 || state>4) throw std::logic_error("Invalid mob stack state projection");
}
}
std::span<const MobAiTransition> mob_ai_transitions() { return table; }
void initialize_mob_ai_stack(MobRuntime& mob,std::int32_t initial,MobAiStateServices& services) {
    if(initial<0 || initial>4) {
        if(initial!=-1 && mob.state_stack) mob.state_stack->clear();
        services.error(mob,MobAiError::invalid_initial,initial,-1);
        // Invalid -1 is rejected before clearing; absent registered state clears
        // and releases the FSM. These are distinct original failure paths.
        if(initial!=-1) mob.state_stack.reset();
        return;
    }
    mob.state_stack=std::vector<std::int32_t>{initial};
}
void dispatch_mob_ai_event(MobRuntime& mob,std::int32_t event,std::uint32_t argument,MobAiStateServices& services) {
    validate(mob);
    if(mob.state_stack->empty()) {
        services.error(mob,MobAiError::empty_dispatch,-1,event);
        initialize_mob_ai_stack(mob,0,services); return;
    }
    const auto state=mob.state_stack->back();
    const MobAiTransition* transition=nullptr;
    for(const auto& item:table) if(item.from==state && item.event==event) {transition=&item;break;}
    if(!transition) {services.error(mob,MobAiError::unhandled_event,state,event);return;}
    dispatch_mob_event(mob,transition->action,argument,services);
    // Lua may synchronously dispatch another event; operate on the live stack.
    validate(mob);
    switch(transition->operation) {
    case O::push:
        mob.state_stack->push_back(transition->to);
        if(mob.state_stack->size()>5) {
            services.error(mob,MobAiError::overflow,state,event);
            initialize_mob_ai_stack(mob,0,services);
        }
        break;
    case O::pop:
        if(!mob.state_stack->empty()) mob.state_stack->pop_back();
        if(mob.state_stack->empty()) {
            services.error(mob,MobAiError::empty_pop,state,event);
            initialize_mob_ai_stack(mob,0,services);
        }
        break;
    case O::replace:
        if(!mob.state_stack->empty()) mob.state_stack->pop_back();
        mob.state_stack->push_back(transition->to); break;
    }
}
bool poll_mob_ai_stack(MobRuntime& mob,MobAiStateServices& services) {
    validate(mob);
    if(mob.state_stack->empty()) {services.error(mob,MobAiError::empty_poll,-1,-1);return false;}
    const auto state=static_cast<MobAiState>(mob.state_stack->back());
    if(state!=MobAiState::death) services.update(mob,state);
    return true;
}
}
