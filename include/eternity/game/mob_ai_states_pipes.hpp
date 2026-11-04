#pragma once
#include "eternity/game/mob_ai_states_arguments.hpp"
namespace eternity::game {
class MobAiArgumentDispatcher {
public:
    virtual ~MobAiArgumentDispatcher()=default;
    virtual MobAiParameter invoke(MobAiConfiguredFsm&,std::uint32_t,std::vector<MobAiParameter>& arguments)=0;
};
// These six actual CMob queries ignore the vector, even if metadata requests
// arguments. This adapter does not provide other world-function implementations.
class MobAiQueryArguments final:public MobAiArgumentDispatcher {
public:
    explicit MobAiQueryArguments(MobAiSystemDispatcher& queries):queries_(queries){}
    MobAiParameter invoke(MobAiConfiguredFsm& owner,std::uint32_t id,std::vector<MobAiParameter>&) override{return queries_.invoke(owner,id);}
private:MobAiSystemDispatcher& queries_;
};
std::int32_t interpret_mob_ai_piped_calls(MobAiConfiguredFsm&,MobAiConfiguredCondition&,MobAiSystemCatalog&,MobAiArgumentDispatcher&);
class MobAiPipedServices final:public MobAiConfiguredServices {
public:
    MobAiPipedServices(MobAiSystemCatalog& definitions,MobAiArgumentDispatcher& dispatcher):definitions_(definitions),dispatcher_(dispatcher){}
    std::int32_t travel_condition(MobRuntime&,MobAiConfiguredFsm& owner,MobAiConfiguredCondition& condition) override{return interpret_mob_ai_piped_calls(owner,condition,definitions_,dispatcher_);}
private:MobAiSystemCatalog& definitions_;MobAiArgumentDispatcher& dispatcher_;
};
}
