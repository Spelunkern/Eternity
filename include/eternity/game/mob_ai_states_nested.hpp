#pragma once
#include "eternity/game/mob_ai_states_argument_math.hpp"
namespace eternity::game {
// Recursive CalculateParam calls share cursor/local definitions while each
// argument retains its own private stack and owned function-argument vector.
std::int32_t interpret_mob_ai_nested_calls(MobAiConfiguredFsm&,MobAiConfiguredCondition&,MobAiSystemCatalog&,MobAiArgumentDispatcher&);
class MobAiNestedServices final:public MobAiConfiguredServices {
public:
    MobAiNestedServices(MobAiSystemCatalog& definitions,MobAiArgumentDispatcher& dispatcher):definitions_(definitions),dispatcher_(dispatcher){}
    std::int32_t travel_condition(MobRuntime&,MobAiConfiguredFsm& owner,MobAiConfiguredCondition& condition) override{return interpret_mob_ai_nested_calls(owner,condition,definitions_,dispatcher_);}
private:MobAiSystemCatalog& definitions_;MobAiArgumentDispatcher& dispatcher_;
};
}
