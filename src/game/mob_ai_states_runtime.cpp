#include "eternity/game/mob_ai_states_runtime.hpp"
#include <stdexcept>
namespace eternity::game {
void debug_mob_ai(MobAiDebugServices& debug){debug.write("PAI Debug on\n");}
void MobAiCompiledRuntime::invoke(MobAiCompiledFunction function,MobRuntime& mob,MobAiConfiguredFsm& fsm){
    switch(function){
    case MobAiCompiledFunction::move_random:update_mob_ai_random(mob,random_definition_,map_,false,movement_);return;
    case MobAiCompiledFunction::move_random_run:update_mob_ai_random(mob,random_definition_,map_,true,movement_);return;
    case MobAiCompiledFunction::move_selected:update_mob_ai_selected(mob,map_,movement_);return;
    case MobAiCompiledFunction::move_run_away:update_mob_ai_run_away(mob,map_,movement_);return;
    case MobAiCompiledFunction::battle:update_mob_ai_battle(mob,fsm,battle_definition_,catalog_,battle_);return;
    case MobAiCompiledFunction::debug:debug_mob_ai(debug_);return;
    }
    throw std::logic_error("Unknown compiled mob AI callback");
}
bool start_mob_ai_factory(MobRuntime& mob,MobAiConfiguredFsm& fsm,std::string_view start_name,
    std::span<const MobAiCompiledStateNames> bindings,MobAiCompiledServices& compiled,MobAiConfiguredServices& conditions){
    if(start_name.substr(0,start_name.find('\0')).size()>=128)throw std::logic_error("Mob AI start name lacks a retained 128-byte terminator");
    fsm.start=mob_ai_name_code(start_name);
    connect_mob_ai_functions(mob,fsm,bindings,compiled);
    return start_mob_ai_configured(mob,fsm,conditions);
}
}
