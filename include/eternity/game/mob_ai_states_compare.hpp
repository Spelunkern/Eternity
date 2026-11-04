#pragma once
#include "eternity/game/mob_ai_states_locals_vm.hpp"
namespace eternity::game {
// Kind 9 comparisons 0..5, restricted to admitted int/DWORD operands, plus
// existing literal/local branches. Float/string comparisons remain explicit errors.
std::int32_t interpret_mob_ai_integer_conditions(MobAiConfiguredFsm&,MobAiConfiguredCondition&);
class MobAiIntegerConditionServices final:public MobAiConfiguredServices {
public:
    std::int32_t travel_condition(MobRuntime&,MobAiConfiguredFsm& owner,MobAiConfiguredCondition& condition) override {return interpret_mob_ai_integer_conditions(owner,condition);}
};
}
