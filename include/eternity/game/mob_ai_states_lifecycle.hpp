#pragma once
#include "eternity/game/mob_ai_states.hpp"
namespace eternity::game {
enum class MobAiLuaLibrary {base,math,os};
// Required VM owner. Calls must execute actual Lua operations, handle script
// errors according to the original contract and retain the MobRuntime storage.
class MobAiLuaServices {
public:
    virtual ~MobAiLuaServices()=default;
    virtual void* create(MobRuntime&)=0;
    virtual void open_library(MobRuntime&,void* state,MobAiLuaLibrary)=0;
    // Original LuaMob registration, CMob global and Data/Lua/<mob type>.lua load.
    virtual void register_mob(MobRuntime&,void* state)=0;
    virtual void close(MobRuntime&,void* state)=0;
    virtual void call_init(MobRuntime&,void* state)=0; // Lua Init(), no arguments.
};
// ps_game.exe InitLua 004b14f0 / ReleaseLua 004b1530. These retain Lua/FSM flags.
bool initialize_mob_ai_vm(MobRuntime&,MobAiLuaServices&);
void release_mob_ai_vm(MobRuntime&,MobAiLuaServices&);
// InitLuaFSM 004b4c20 / ReleaseLuaFSM 004b5260. Existing state_stack represents
// active stack ownership. Initialization does not implicitly release an old VM.
void initialize_mob_ai_lua_fsm(MobRuntime&,MobAiLuaServices&,MobAiStateServices&);
void release_mob_ai_lua_fsm(MobRuntime&,MobAiLuaServices&);
// LuaError 004b52b0 clears enable/only-attack flags, retaining VM and FSM.
void disable_mob_ai_lua(MobRuntime&) noexcept;
// LuaInitCall 004b1550 checks only lua_ai, not pointer presence.
void call_mob_ai_lua_init(MobRuntime&,MobAiLuaServices&);
}
