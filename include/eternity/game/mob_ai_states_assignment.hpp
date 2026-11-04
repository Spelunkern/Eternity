#pragma once
#include "eternity/game/mob_ai_states_arithmetic.hpp"
namespace eternity::game {
// TravelCondition opcode 10: int/DWORD assignment, retaining second operand
// metadata and writing through its embedded VariableID after the result push.
// New map entries, float assignments and compound updates remain explicit errors.
std::int32_t interpret_mob_ai_integer_assignment(MobAiConfiguredFsm&,MobAiConfiguredCondition&);
class MobAiIntegerAssignmentServices final:public MobAiConfiguredServices {
public:
    std::int32_t travel_condition(MobRuntime&,MobAiConfiguredFsm& owner,MobAiConfiguredCondition& condition) override {return interpret_mob_ai_integer_assignment(owner,condition);}
};
}
