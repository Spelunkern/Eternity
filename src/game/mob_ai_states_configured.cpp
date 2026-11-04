#include "eternity/game/mob_ai_states_configured.hpp"
#include <stdexcept>
namespace eternity::game {
namespace {
void phase(MobRuntime& mob,MobAiConfiguredFsm& fsm,MobAiConfiguredProgram& program,MobAiConfiguredAction& action,MobAiConfiguredServices& services){
    if(program.enabled){
        // Enabled is sampled once; count and expression storage reload after
        // every TravelCondition, including callback-driven append/count changes.
        for(std::int32_t i=0;i<program.count;++i){
            if(static_cast<std::size_t>(i)>=program.expressions.size())throw std::logic_error("Configured mob state program exceeds retained expression storage");
            services.travel_condition(mob,fsm,program.expressions[static_cast<std::size_t>(i)]);
        }
    }
    auto invoke=action;if(invoke)invoke(mob,fsm);
}
void transition(MobRuntime& mob,MobAiConfiguredFsm& fsm,std::uint32_t previous,std::uint32_t target,MobAiConfiguredServices& services){
    auto& from=fsm.states[previous];phase(mob,fsm,from.exit_program,from.exit,services);
    auto& to=fsm.states[target];phase(mob,fsm,to.enter_program,to.enter,services);
    fsm.current=target; // Publication follows both callbacks, overwriting changes.
}
}
bool start_mob_ai_configured(MobRuntime& mob,MobAiConfiguredFsm& fsm,MobAiConfiguredServices& services){
    fsm.current=fsm.start;
    auto& state=fsm.states[fsm.current];phase(mob,fsm,state.enter_program,state.enter,services);return true;
}
void execute_mob_ai_configured(MobRuntime& mob,MobAiConfiguredFsm& fsm,MobAiConfiguredServices& services){
    auto& state=fsm.states[fsm.current];phase(mob,fsm,state.update_program,state.update,services);
}
void evaluate_mob_ai_configured(MobRuntime& mob,MobAiConfiguredFsm& fsm,MobAiConfiguredServices& services){
    auto& transitions=fsm.states[fsm.current].transitions;
    for(auto& candidate:transitions){
        if(candidate.conditions.empty() && fsm.current!=candidate.target){
            const auto previous=fsm.current;const auto target=candidate.target;
            transition(mob,fsm,previous,target,services);return;
        }
        std::uint32_t sum=0;
        for(auto& condition:candidate.conditions)sum+=static_cast<std::uint32_t>(services.travel_condition(mob,fsm,condition));
        if(std::bit_cast<std::int32_t>(sum)==candidate.expected_sum && fsm.current!=candidate.target){
            const auto previous=fsm.current;const auto target=candidate.target;
            transition(mob,fsm,previous,target,services);return;
        }
    }
}
}
