#pragma once
#include "eternity/game/mob_ai_states_configured.hpp"
namespace eternity::game {
// TravelCondition 00498cf0: numeric/string literal and ignored-kind subset only.
// Kinds 3/4/8/9 require the unrecovered variable/function/operator interpreter.
// It rejects those explicitly after retaining any already executed prefix.
std::int32_t interpret_mob_ai_literals(MobAiConfiguredFsm&,MobAiConfiguredCondition&);
class MobAiLiteralConditionServices final:public MobAiConfiguredServices {
public:
    std::int32_t travel_condition(MobRuntime&,MobAiConfiguredFsm& owner,MobAiConfiguredCondition& condition) override {return interpret_mob_ai_literals(owner,condition);}
};
}
