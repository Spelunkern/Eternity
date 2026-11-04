#include "eternity/game/mob_events.hpp"

namespace eternity::game {
void dispatch_mob_event(MobRuntime& mob, MobEvent event, std::uint32_t argument, MobEventServices& services) {
    if (event == MobEvent::move) return;
    if (event == MobEvent::move_end) { mob.move_mode = 0; return; }
    if (event == MobEvent::normal_reset) {
        mob.only_lua_attack = 0;
        mob.combat_timeout = 0;
        mob.current_attack = 0;
    }
    if (mob.lua_ai == 0) return;
    std::string_view function;
    std::optional<std::uint32_t> value;
    switch (event) {
    case MobEvent::move_end_lua: function = "OnMoveEnd"; break;
    case MobEvent::attacked: function = "OnAttacked"; mob.function_type = 1; value = argument; break;
    case MobEvent::attackable: function = "OnAttackable"; mob.function_type = 2; value = argument; break;
    case MobEvent::normal_reset: function = "OnNormalReset"; mob.function_type = 3; break;
    case MobEvent::death: function = "OnDeath"; mob.function_type = 4; value = mob.lua_attacked_count; break;
    case MobEvent::return_home: function = "OnReturnHome"; mob.function_type = 5; value = mob.lua_attacked_count; break;
    default: return;
    }
    const auto now = services.tick();
    services.lua_call(mob, function, now, value);
    // The original always clears the discriminator, even if the callback changed it.
    // The no-argument OnMoveEnd overload neither sets nor clears it.
    if (event != MobEvent::move_end_lua) mob.function_type = 0;
}
}
