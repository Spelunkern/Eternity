#pragma once
#include "eternity/game/mobs.hpp"
#include "eternity/game/mob_ai_states_parameters.hpp"
#include "eternity/game/mob_ai_states_tokens.hpp"
#include "eternity/game/mob_ai_states_values.hpp"
#include <array>
#include <bit>
#include <functional>
#include <map>
#include <string>
namespace eternity::game {
struct MobAiConfiguredFsm;
// KFSMCondition is seven DWORDs (0x1c). The first three are type/count/cursor;
// legacy words remain an explicit projection, never native vector pointers.
// Tokens are decoded owned values. TravelCondition remains a required provider.
struct MobAiConfiguredCondition {
    std::array<std::uint32_t,7> words{};
    std::vector<MobAiToken> tokens{};
};
struct MobAiConfiguredTransition {
    std::uint32_t target=0;
    std::int32_t expected_sum=0;
    std::vector<MobAiConfiguredCondition> conditions;
    std::int32_t id=0,calc_count=0;
    std::string start_text{},end_text{},output_text{};
};
using MobAiConfiguredAction=std::function<void(MobRuntime&,MobAiConfiguredFsm&)>;
struct MobAiConfiguredProgram {
    bool enabled=false;
    std::int32_t count=0;
    std::vector<MobAiConfiguredCondition> expressions;
    std::string text{};
};
struct MobAiConfiguredState {
    // Empty callbacks preserve the original null member-function slots.
    MobAiConfiguredAction enter,update,exit;
    MobAiConfiguredProgram enter_program,update_program,exit_program;
    std::vector<MobAiConfiguredTransition> transitions;
    std::string name{};
    std::uint32_t state_code=0;
    // Original pKFSM (+f4) is borrowed and copied, never rebound by AddState.
    // Null keeps the legacy direct-input projection bound to its containing FSM.
    MobAiConfiguredFsm* condition_owner=nullptr;
};
struct MobAiSignedCodeLess {bool operator()(std::uint32_t a,std::uint32_t b) const noexcept{return std::bit_cast<std::int32_t>(a)<std::bit_cast<std::int32_t>(b);}};
struct MobAiConfiguredFsm {
    // Explicit inputs: no guessed constructor/start-code defaults.
    MobAiConfiguredFsm(std::uint32_t current_code,std::uint32_t start_code):current(current_code),start(start_code){}
    std::uint32_t current,start;
    MobAiVariables variables;
    MobAiValueStack value_stack;
    // CMob retained CFunctionPointerList (+114): ConnectToFSM appends each call.
    std::vector<std::pair<std::string,MobAiConfiguredAction>> compiled_functions;
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
