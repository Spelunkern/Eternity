#pragma once
#include "eternity/game/mob_ai_states_compare.hpp"
namespace eternity::game {
// Numeric int/float/DWORD comparison matrix (kind 9, opcodes 0..5), plus
// recovered literal/local branches. Strings/other value types remain errors.
std::int32_t interpret_mob_ai_numeric_conditions(MobAiConfiguredFsm&,MobAiConfiguredCondition&);
class MobAiNumericConditionServices final:public MobAiConfiguredServices {
public:
    std::int32_t travel_condition(MobRuntime&,MobAiConfiguredFsm& owner,MobAiConfiguredCondition& condition) override {return interpret_mob_ai_numeric_conditions(owner,condition);}
};
}
