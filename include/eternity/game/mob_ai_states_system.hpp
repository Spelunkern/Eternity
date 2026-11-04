#pragma once
#include "eternity/game/mob_ai_states_finite_compound.hpp"
namespace eternity::game {
// CKFSM +58 SysFuncMap is distinct from the compiled state-action registry.
// CKSystemFunction +90 vector entries are {type, pipe}; F_OUT is exactly 1.
struct MobAiSystemParameter {std::int32_t type=0,pipe=0;};
struct MobAiSystemDefinition {std::int32_t parameter_count=0;std::vector<MobAiSystemParameter> parameters{};};
using MobAiSystemCatalog=std::map<std::uint32_t,MobAiSystemDefinition,MobAiSignedCodeLess>;
class MobAiSystemDispatcher {
public:
    virtual ~MobAiSystemDispatcher()=default;
    virtual MobAiParameter invoke(MobAiConfiguredFsm&,std::uint32_t function)=0;
};
// Explicit projection of CKFSM +48 m_pInstance onto the actual MobRuntime.
// Six recovered no-argument queries; unknown IDs require another provider.
class MobAiMobQueries final:public MobAiSystemDispatcher {
public:
    MobAiMobQueries(MobAiConfiguredFsm& owner,MobRuntime& subject):owner_(owner),subject_(subject){}
    MobAiParameter invoke(MobAiConfiguredFsm&,std::uint32_t) override;
private:MobAiConfiguredFsm& owner_;MobRuntime& subject_;
};
std::int32_t interpret_mob_ai_zero_argument_calls(MobAiConfiguredFsm&,MobAiConfiguredCondition&,MobAiSystemCatalog&,MobAiSystemDispatcher&);
class MobAiZeroArgumentServices final:public MobAiConfiguredServices {
public:
    MobAiZeroArgumentServices(MobAiSystemCatalog& catalog,MobAiSystemDispatcher& dispatcher):catalog_(catalog),dispatcher_(dispatcher){}
    std::int32_t travel_condition(MobRuntime&,MobAiConfiguredFsm& owner,MobAiConfiguredCondition& condition) override {return interpret_mob_ai_zero_argument_calls(owner,condition,catalog_,dispatcher_);}
private:MobAiSystemCatalog& catalog_;MobAiSystemDispatcher& dispatcher_;
};
}
