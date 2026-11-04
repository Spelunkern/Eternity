#pragma once
#include "eternity/game/mob_ai_states_literals.hpp"
namespace eternity::game {
// TravelCondition 00498e24: local reads plus the recovered literal subset.
// Locals must already be admitted typed values; no invented missing-map default.
std::int32_t interpret_mob_ai_local_values(MobAiConfiguredFsm&,MobAiConfiguredCondition&);
class MobAiLocalValueConditionServices final:public MobAiConfiguredServices {
public:
    std::int32_t travel_condition(MobRuntime&,MobAiConfiguredFsm& owner,MobAiConfiguredCondition& condition) override {return interpret_mob_ai_local_values(owner,condition);}
};
}
