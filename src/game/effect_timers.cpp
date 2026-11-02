#include "eternity/game/effect_timers.hpp"
#include <algorithm>

namespace eternity::game {
int effect_map_result(std::uint16_t map,std::uint16_t id,std::uint8_t level) {
    // EnableApplySkillByMap 004845f0, ps_game.exe.
    if(map!=18) return 0;
    switch(id) {
        case 111:case 112:case 114:case 116:case 117:case 120:case 121:case 123:case 127:
            return level>1 && level!=100?2:0;
        case 124:case 175:case 181:case 185:return 2;
    }
    return 0;
}
bool EffectTimer::expired(std::uint32_t tick) const {
    // CUser::ChkEndTimeSkill 004865b0 uses unsigned absolute deadlines, including timer wrap behavior.
    return end<=(offline_countdown?tick/1000U:tick);
}
RestoredEffects restore_effects(const std::vector<PersistedEffect>& effects,const SkillDefinitions& definitions,
    std::uint16_t map,std::uint32_t disconnected,std::uint32_t tick) {
    // CUser::AddApplySkill 00487ce0, ps_game.exe: DB restoration, before applying effect state.
    RestoredEffects result;
    for(const auto& effect:effects) {
        const auto* skill=definitions.find(effect.skill_id,effect.level);
        if(!skill || (skill->effect!=3 && skill->effect!=4) || effect_map_result(map,skill->id,skill->level)!=0) continue;
        if(skill->offline_countdown && effect.remaining<disconnected) {result.erase.push_back(effect);continue;}
        EffectTimer timer{effect,tick,0,skill->offline_countdown};
        timer.attacker_kind=effect.level==100?2:1;
        if(skill->offline_countdown) timer.end=std::min(tick/1000U-disconnected+effect.remaining+10U,tick/1000U+skill->keep_time_seconds);
        else timer.end=std::min(tick+(effect.remaining+10U)*1000U,tick+skill->keep_time_seconds*1000U);
        result.active.push_back(timer);
    }
    return result;
}
}
