#pragma once
#include "eternity/game/mob_ai_states_finite_arithmetic.hpp"
namespace eternity::game {
// Compound opcodes 11..16 with finite floating operands, retaining original
// two pops, staged arithmetic, result push and embedded VariableID writeback.
std::int32_t interpret_mob_ai_finite_compound(MobAiConfiguredFsm&,MobAiConfiguredCondition&);
class MobAiFiniteCompoundServices final:public MobAiConfiguredServices {
public:
    std::int32_t travel_condition(MobRuntime&,MobAiConfiguredFsm& owner,MobAiConfiguredCondition& condition) override {return interpret_mob_ai_finite_compound(owner,condition);}
};
}
