#include "eternity/game/mob_ai_states_bindings.hpp"
#include "eternity/game/mob_ai_states_reset.hpp"
#include <mutex>
#include <stdexcept>
namespace eternity::game {
namespace {
std::string_view terminated(std::string_view value){return value.substr(0,value.find('\0'));}
void validate(std::string_view value){if(terminated(value).size()>=128)throw std::logic_error("Mob AI binding name lacks a retained 128-byte terminator");}
}
std::uint32_t mob_ai_name_code(std::string_view text) noexcept {
    std::uint32_t code=5381;
    for(unsigned char byte:text){
        if(byte==0)break;
        const auto signed_byte=byte<128?static_cast<std::int32_t>(byte):static_cast<std::int32_t>(byte)-256;
        code=code*33U+static_cast<std::uint32_t>(signed_byte);
    }
    return code;
}
void connect_mob_ai_functions(MobRuntime& subject,MobAiConfiguredFsm& fsm,
    std::span<const MobAiCompiledStateNames> states,MobAiCompiledServices& services){
    static std::recursive_mutex binding_lock;
    const std::lock_guard lock(binding_lock);
    const auto add=[&](const char* name,MobAiCompiledFunction kind){
        fsm.compiled_functions.emplace_back(name,[&services,kind](MobRuntime& mob,MobAiConfiguredFsm& owner){services.invoke(kind,mob,owner);});
    };
    add("MoveRandom",MobAiCompiledFunction::move_random);
    add("MoveRandomRun",MobAiCompiledFunction::move_random_run);
    add("MoveAtSelectedPosition",MobAiCompiledFunction::move_selected);
    add("MoveRunAway",MobAiCompiledFunction::move_run_away);
    add("Battle",MobAiCompiledFunction::battle);
    add("Debug",MobAiCompiledFunction::debug);
    fsm.compiled_functions.emplace_back("ResetUser",[](MobRuntime& mob,MobAiConfiguredFsm&){reset_mob_ai_user(mob);});
    for(const auto& names:states){
        validate(names.state);validate(names.enter);validate(names.update);validate(names.exit);
        MobAiConfiguredAction enter,update,exit;
        for(const auto& [name,action]:fsm.compiled_functions){
            if(terminated(name)==terminated(names.enter))enter=action;
            if(terminated(name)==terminated(names.update))update=action;
            if(terminated(name)==terminated(names.exit))exit=action;
        }
        auto& state=fsm.states[mob_ai_name_code(names.state)];
        const auto bind=[&subject](MobAiConfiguredAction action)->MobAiConfiguredAction {
            if(!action)return {};
            return [&subject,action=std::move(action)](MobRuntime&,MobAiConfiguredFsm& owner){action(subject,owner);};
        };
        state.enter=bind(std::move(enter));state.update=bind(std::move(update));state.exit=bind(std::move(exit));
    }
}
}
