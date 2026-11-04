#pragma once
#include "eternity/game/mob_ai_states_numeric.hpp"
namespace eternity::game {
// Kind 8 binary opcodes 6..9 for int/DWORD, plus recovered values/comparisons.
// Float arithmetic, assignment/control opcodes 10..16 and system calls remain errors.
std::int32_t interpret_mob_ai_integer_arithmetic(MobAiConfiguredFsm&,MobAiConfiguredCondition&);
class MobAiIntegerArithmeticServices final:public MobAiConfiguredServices {
public:
    std::int32_t travel_condition(MobRuntime&,MobAiConfiguredFsm& owner,MobAiConfiguredCondition& condition) override {return interpret_mob_ai_integer_arithmetic(owner,condition);}
};
}
