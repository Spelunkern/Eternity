#pragma once
#include "eternity/game/mob_ai_states_nested.hpp"
namespace eternity::game {
// CalculateParam int/DWORD assignment/compound opcodes 10..16. Writes use the
// copied second operand's VariableID, including zero, on the existing map.
void calculate_mob_ai_writing_argument(MobAiConfiguredFsm&,MobAiConfiguredCondition&,MobAiParameter&);
std::int32_t interpret_mob_ai_writing_calls(MobAiConfiguredFsm&,MobAiConfiguredCondition&,MobAiSystemCatalog&,MobAiArgumentDispatcher&);
class MobAiWritingServices final:public MobAiConfiguredServices {
public:
    MobAiWritingServices(MobAiSystemCatalog& definitions,MobAiArgumentDispatcher& dispatcher):definitions_(definitions),dispatcher_(dispatcher){}
    std::int32_t travel_condition(MobRuntime&,MobAiConfiguredFsm& owner,MobAiConfiguredCondition& condition) override{return interpret_mob_ai_writing_calls(owner,condition,definitions_,dispatcher_);}
private:MobAiSystemCatalog& definitions_;MobAiArgumentDispatcher& dispatcher_;
};
}
