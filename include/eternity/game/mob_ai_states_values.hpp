#pragma once
#include "eternity/game/mob_ai_states_parameters.hpp"
namespace eternity::game {
// Original KStack<AI_Param> retained in CKFSM: capacity +3c, top +40, data +44.
struct MobAiValueStack {
    // CKFSM constructor 004951b0 passes 64 at 00495226. Unassigned slot
    // type/text bytes are normalized; original AI_Param[] leaves them undefined.
    MobAiValueStack():slots(64,MobAiParameter(0)){}
    std::int32_t capacity=64,top=-1;
    std::vector<MobAiParameter> slots;
};
}
