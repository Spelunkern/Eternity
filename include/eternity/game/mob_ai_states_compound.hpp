#pragma once
#include "eternity/game/mob_ai_states_assignment.hpp"
namespace eternity::game {
// Kind 8 opcodes 11..16: integer compound writes, including two-pop increment
// and decrement. Existing retained writeback targets are required.
std::int32_t interpret_mob_ai_integer_compound(MobAiConfiguredFsm&,MobAiConfiguredCondition&);
class MobAiIntegerCompoundServices final:public MobAiConfiguredServices {
public:
    std::int32_t travel_condition(MobRuntime&,MobAiConfiguredFsm& owner,MobAiConfiguredCondition& condition) override {return interpret_mob_ai_integer_compound(owner,condition);}
};
}
