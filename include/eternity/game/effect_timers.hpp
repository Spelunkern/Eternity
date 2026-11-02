#pragma once
#include "eternity/game/skills.hpp"

namespace eternity::game {
struct EffectTimer {
    PersistedEffect effect;
    std::uint32_t previous_tick = 0,end = 0;
    bool offline_countdown = false;
    std::uint8_t attacker_kind = 1;
    std::uint32_t attacker_id = 0;
    bool expired(std::uint32_t tick_ms) const;
};
struct RestoredEffects {
    std::vector<EffectTimer> active;
    // Original loading requests SQL deletion only for offline timers already spent while disconnected.
    std::vector<PersistedEffect> erase;
};
int effect_map_result(std::uint16_t map,std::uint16_t skill_id,std::uint8_t level);
RestoredEffects restore_effects(const std::vector<PersistedEffect>& effects,const SkillDefinitions& definitions,
    std::uint16_t map,std::uint32_t disconnected_seconds,std::uint32_t tick_ms);
}
