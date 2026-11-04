#pragma once
#include "eternity/game/mob_ai_states_compound.hpp"
namespace eternity::game {
// Full int/float/DWORD assignment opcode 10, including masked x87 FISTP
// indefinite results and float payload copying. Other float arithmetic errors.
std::int32_t interpret_mob_ai_numeric_assignment(MobAiConfiguredFsm&,MobAiConfiguredCondition&);
class MobAiNumericAssignmentServices final:public MobAiConfiguredServices {
public:
    std::int32_t travel_condition(MobRuntime&,MobAiConfiguredFsm& owner,MobAiConfiguredCondition& condition) override {return interpret_mob_ai_numeric_assignment(owner,condition);}
};
}
