#pragma once
#include "eternity/game/mob_ai_states_numeric_assignment.hpp"
namespace eternity::game {
// Binary opcodes 6..9 with finite floating operands, staged float32 coercion
// and x87 53-bit arithmetic. Nonfinite input propagation remains an error.
std::int32_t interpret_mob_ai_finite_arithmetic(MobAiConfiguredFsm&,MobAiConfiguredCondition&);
class MobAiFiniteArithmeticServices final:public MobAiConfiguredServices {
public:
    std::int32_t travel_condition(MobRuntime&,MobAiConfiguredFsm& owner,MobAiConfiguredCondition& condition) override {return interpret_mob_ai_finite_arithmetic(owner,condition);}
};
}
