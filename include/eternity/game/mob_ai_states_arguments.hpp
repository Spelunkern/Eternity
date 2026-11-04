#pragma once
#include "eternity/game/mob_ai_states_system.hpp"
namespace eternity::game {
// CalculateParam 0049d020: private 20-value stack, shared condition cursor.
// Kind 5 copies the top to retained output; exhausted count leaves output intact.
// Supports literal/local/string values; operators and nested calls require the
// remaining original CalculateParam provider and explicitly error.
void calculate_mob_ai_argument(MobAiConfiguredFsm&,MobAiConfiguredCondition&,MobAiParameter& output);
}
