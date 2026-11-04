#pragma once
#include "eternity/game/mob_ai_states_pipes.hpp"
namespace eternity::game {
// CalculateParam kind-8 integer/DWORD binary arithmetic (6..9), on its private
// stack. Float arithmetic, assignments and nested calls remain explicit errors.
void calculate_mob_ai_arithmetic_argument(MobAiConfiguredFsm&,MobAiConfiguredCondition&,MobAiParameter&);
std::int32_t interpret_mob_ai_expression_calls(MobAiConfiguredFsm&,MobAiConfiguredCondition&,MobAiSystemCatalog&,MobAiArgumentDispatcher&);
class MobAiExpressionServices final:public MobAiConfiguredServices {
public:
    MobAiExpressionServices(MobAiSystemCatalog& definitions,MobAiArgumentDispatcher& dispatcher):definitions_(definitions),dispatcher_(dispatcher){}
    std::int32_t travel_condition(MobRuntime&,MobAiConfiguredFsm& owner,MobAiConfiguredCondition& condition) override{return interpret_mob_ai_expression_calls(owner,condition,definitions_,dispatcher_);}
private:MobAiSystemCatalog& definitions_;MobAiArgumentDispatcher& dispatcher_;
};
}
