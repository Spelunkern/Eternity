#include "eternity/game/mob_ai_states_lifecycle.hpp"
namespace eternity::game {
bool initialize_mob_ai_vm(MobRuntime& mob,MobAiLuaServices& vm){
    mob.lua_state=vm.create(mob);
    if(!mob.lua_state)return false;
    vm.open_library(mob,mob.lua_state,MobAiLuaLibrary::base);
    vm.open_library(mob,mob.lua_state,MobAiLuaLibrary::math);
    vm.open_library(mob,mob.lua_state,MobAiLuaLibrary::os);
    return true;
}
void release_mob_ai_vm(MobRuntime& mob,MobAiLuaServices& vm){
    if(mob.lua_state){vm.close(mob,mob.lua_state);mob.lua_state=nullptr;}
}
void initialize_mob_ai_lua_fsm(MobRuntime& mob,MobAiLuaServices& vm,MobAiStateServices& states){
    mob.retarget_time=5000;mob.lua_attacked_count=0;mob.only_lua_attack=0;
    mob.hold_position=0;mob.lua_order_time=0;mob.move_mode=0;
    if(!mob.state_stack)mob.state_stack.emplace();
    mob.move_target={};mob.function_type=0;
    if(initialize_mob_ai_vm(mob,vm)){
        mob.lua_ai=1;
        vm.register_mob(mob,mob.lua_state);
    }
    // Fixed table installation/initial selection still occurs on VM allocation
    // failure or after a script error disabled Lua. It does not reset lua_ai.
    initialize_mob_ai_stack(mob,0,states);
}
void release_mob_ai_lua_fsm(MobRuntime& mob,MobAiLuaServices& vm){
    mob.state_stack.reset(); // ReleaseFSM runs before Lua flag checks / close GC.
    if(mob.lua_ai!=0){mob.lua_ai=0;mob.only_lua_attack=0;}
    release_mob_ai_vm(mob,vm);
}
void disable_mob_ai_lua(MobRuntime& mob) noexcept {mob.only_lua_attack=0;mob.lua_ai=0;}
void call_mob_ai_lua_init(MobRuntime& mob,MobAiLuaServices& vm){if(mob.lua_ai!=0)vm.call_init(mob,mob.lua_state);}
}
