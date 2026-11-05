#pragma once
#include "eternity/game/mob_ai_states_argument_writes.hpp"
namespace eternity::game {
// CalculateParam assignment opcode 10: complete int/float/DWORD conversion
// matrix, preserving masked FISTP results and raw float-to-float payloads.
void calculate_mob_ai_numeric_writing_argument(MobAiConfiguredFsm&,MobAiConfiguredCondition&,MobAiParameter&);
std::int32_t interpret_mob_ai_numeric_argument_calls(MobAiConfiguredFsm&,MobAiConfiguredCondition&,MobAiSystemCatalog&,MobAiArgumentDispatcher&);
class MobAiNumericArgumentServices final:public MobAiConfiguredServices {
public:
    MobAiNumericArgumentServices(MobAiSystemCatalog& definitions,MobAiArgumentDispatcher& dispatcher):definitions_(definitions),dispatcher_(dispatcher){}
    std::int32_t travel_condition(MobRuntime&,MobAiConfiguredFsm& owner,MobAiConfiguredCondition& condition) override{return interpret_mob_ai_numeric_argument_calls(owner,condition,definitions_,dispatcher_);}
private:MobAiSystemCatalog& definitions_;MobAiArgumentDispatcher& dispatcher_;
};
}
