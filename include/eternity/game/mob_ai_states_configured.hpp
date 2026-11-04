#pragma once
#include "eternity/game/mobs.hpp"
#include "eternity/game/mob_ai_states_parameters.hpp"
#include <array>
#include <bit>
#include <functional>
#include <map>
namespace eternity::game {
struct MobAiConfiguredFsm;
// KFSMCondition is seven DWORDs (0x1c). TravelCondition's expression/token
// interpreter remains a required provider; these words are retained verbatim.
struct MobAiConfiguredCondition {std::array<std::uint32_t,7> words{};};
struct MobAiConfiguredTransition {
    std::uint32_t target=0;
    std::int32_t expected_sum=0;
    std::vector<MobAiConfiguredCondition> conditions;
};
using MobAiConfiguredAction=std::function<void(MobRuntime&,MobAiConfiguredFsm&)>;
struct MobAiConfiguredProgram {
    bool enabled=false;
    std::int32_t count=0;
    std::vector<MobAiConfiguredCondition> expressions;
};
struct MobAiConfiguredState {
    // Empty callbacks preserve the original null member-function slots.
    MobAiConfiguredAction enter,update,exit;
    MobAiConfiguredProgram enter_program,update_program,exit_program;
    std::vector<MobAiConfiguredTransition> transitions;
};
struct MobAiSignedCodeLess {bool operator()(std::uint32_t a,std::uint32_t b) const noexcept{return std::bit_cast<std::int32_t>(a)<std::bit_cast<std::int32_t>(b);}};
struct MobAiConfiguredFsm {
    // Explicit inputs: no guessed constructor/start-code defaults.
    MobAiConfiguredFsm(std::uint32_t current_code,std::uint32_t start_code):current(current_code),start(start_code){}
    std::uint32_t current,start;
    MobAiVariables variables;
    std::map<std::uint32_t,MobAiConfiguredState,MobAiSignedCodeLess> states;
};
class MobAiConfiguredServices {
public:
    virtual ~MobAiConfiguredServices()=default;
    virtual std::int32_t travel_condition(MobRuntime&,MobAiConfiguredFsm&,MobAiConfiguredCondition&)=0;
};
// CKFSM<CMob>::Start 004954c0, ExecuteStateMachine 004a8520,
// EvaluateCurrentStateTransitions 004a83e0. State/transition/condition storage
// and subject lifetime must survive callbacks. Transition ranges stay stable;
// phase expression storage/count reload between condition calls.
bool start_mob_ai_configured(MobRuntime&,MobAiConfiguredFsm&,MobAiConfiguredServices&);
void execute_mob_ai_configured(MobRuntime&,MobAiConfiguredFsm&,MobAiConfiguredServices&);
void evaluate_mob_ai_configured(MobRuntime&,MobAiConfiguredFsm&,MobAiConfiguredServices&);
}
