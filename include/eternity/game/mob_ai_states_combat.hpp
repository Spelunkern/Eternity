#pragma once
#include "eternity/game/mobs.hpp"
namespace eternity::game {
// Live projection returned by the serialized user owner. Retain storage through
// the call; life_status is checked after find_user returns (1 means dead).
struct MobAiCombatTarget {std::uint32_t id=0;std::int32_t life_status=0;};
class MobAiCombatServices {
public:
    virtual ~MobAiCombatServices()=default;
    virtual std::uint32_t tick()=0;
    virtual MobAiCombatTarget* find_user(MobRuntime&,std::uint32_t target_id)=0;
    virtual bool attack_ai(MobRuntime&,std::uint32_t now,MobAiCombatTarget&)=0;
    virtual void remove_vision_fix(MobRuntime&)=0;
    virtual void retarget(MobRuntime&,std::uint32_t now)=0;
    // CMob::LuaCall 004b1210: WhileCombat(tick, unsigned HP percentage,
    // attacked count), using the live mob.lua_state; provider handles Lua errors.
    virtual void lua_combat(MobRuntime&,std::uint32_t now,std::uint32_t hp_percent,std::uint32_t attacked_count)=0;
};
// ps_game.exe CMob::WhileCombat 004b55c0. Invoke from the combat state's
// MobAiStateServices::update implementation; this does not replace AttackAIExp.
void update_mob_ai_combat(MobRuntime&,MobAiCombatServices&);
}
