#include "eternity/game/mob_ai_states_combat.hpp"
#include <cmath>
#include <stdexcept>
namespace eternity::game {
namespace {
std::uint32_t percentage(std::uint32_t hp,std::uint32_t maximum) {
    // Windows x87 default precision is 53 bits; FISTP qword truncates, then only
    // the low DWORD is passed to Lua. Invalid conversion yields low DWORD zero.
    const double ratio=static_cast<double>(hp)/static_cast<double>(maximum);
    const double scaled=ratio*100.0;
    if(!std::isfinite(scaled)) return 0;
    return static_cast<std::uint32_t>(static_cast<std::uint64_t>(scaled));
}
}
void update_mob_ai_combat(MobRuntime& mob,MobAiCombatServices& services) {
    const auto now=services.tick();
    if(mob.sleep!=0 || mob.stun!=0) return;
    // 004b55e9..004b5603: the original speed gate reaches its 0xff compare
    // only for 0..6, so it cannot suppress any integer attack speed.
    if(mob.only_lua_attack!=0 || now<mob.next_attack) {
        if(mob.lua_ai!=0) {
            if(!mob.definition) throw std::logic_error("Combat Lua requires the mob HP definition");
            const auto count=mob.lua_attacked_count;
            const auto health=percentage(mob.hp,mob.definition->hp);
            services.lua_combat(mob,now,health,count);
        }
        return;
    }
    auto* target=services.find_user(mob,mob.target_id);
    if(!target || target->life_status==1) {
        services.remove_vision_fix(mob);
        services.retarget(mob,now);
    } else if(!services.attack_ai(mob,now,*target)) services.retarget(mob,now);
}
}
