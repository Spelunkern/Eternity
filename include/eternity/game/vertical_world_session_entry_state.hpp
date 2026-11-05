#pragma once
#include "eternity/game/vertical_world_session.hpp"
#include <bit>

namespace eternity::game {
// Values owned by the live user/session rather than the SQL character. The
// caller must read the actual auto-character and two protocol flag fields.
struct VerticalWorldSessionEntryFlags {
    std::uint32_t auto_character_id;
    std::uint8_t flag_584c,flag_584d;
};
// PacketUserDBSetInfo 0046d780 copies TP_DB_INFO_CHAR offsets 121/122;
// 0046d99c..0046d9d7 clears any nonzero reset-status count and clamps
// reset-skill to one. PlayerCore stores these SQL tinyints as booleans.
inline VerticalWorldSessionEntryFlags vertical_world_session_entry_flags(const PlayerCore& core,std::uint32_t auto_character_id) noexcept {
    return {auto_character_id,0,static_cast<std::uint8_t>(core.reset_skills)};
}
// Project the selected retained actor and SQL character into the 0407 entry
// state. PacketUserDBChar ps_game.exe 0046cf60, success 0046d4c1..0046d717.
inline CharacterEntryState vertical_world_session_entry_state(VerticalWorldSession& session,VerticalWorldSessionEntryFlags flags) {
    auto* character=session.character();auto* user=session.world_user();
    if(session.closing() || session.failed() || !character || !user)throw std::logic_error("Entry state requires a selected session");
    const auto& snapshot=character->snapshot();auto* actor=character->pipeline().result().actor.get();
    if(!actor || &user->actor!=actor || snapshot.core.user_id!=session.account_id() || actor->account_id()!=session.account_id() ||
       actor->id()!=snapshot.core.character.id)throw std::logic_error("Entry state lost its selected actor");
    const auto& current=actor->resources();const auto& maximum=actor->status().skills.equipment.base.maximum;
    CharacterEntryState state;state.user_id=session.account_id();state.character_id=actor->id();state.level=snapshot.core.character.level;
    state.current={std::bit_cast<std::int32_t>(current.hp),std::bit_cast<std::int32_t>(current.mp),std::bit_cast<std::int32_t>(current.sp)};
    state.maximum={std::bit_cast<std::int32_t>(maximum.hp),std::bit_cast<std::int32_t>(maximum.mp),std::bit_cast<std::int32_t>(maximum.sp)};
    state.motion=actor->status().shape;state.guild_present=session.guild_user().guild!=nullptr;
    state.auto_character_id=flags.auto_character_id;state.flag_584c=flags.flag_584c;state.flag_584d=flags.flag_584d;return state;
}
}
