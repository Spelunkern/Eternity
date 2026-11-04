#pragma once
#include "eternity/game/mobs.hpp"
#include <optional>
#include <string_view>

namespace eternity::game {
enum class MobEvent { move, move_end, move_end_lua, attacked, attackable, normal_reset, death, return_home };
// Required production boundary: LuaCall must execute the named function with tick
// and optional argument, using mob.lua_state. Lua errors are handled by the provider.
class MobEventServices {
public:
    virtual ~MobEventServices() = default;
    virtual std::uint32_t tick() = 0;
    virtual void lua_call(MobRuntime&, std::string_view function, std::uint32_t tick,
                          std::optional<std::uint32_t> argument) = 0;
};
// ps_game.exe CMob::OnMove..OnReturnHome 004b60d0..004b6270.
// argument projects EventObject::m_nArgument (+8), not an object pointer.
void dispatch_mob_event(MobRuntime&, MobEvent, std::uint32_t argument, MobEventServices&);
}
