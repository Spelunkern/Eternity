#pragma once
#include <cstdint>
namespace eternity::game {
// Canonical per-guild fields used by CGuild::PvPLeaveGuild 00438160.
struct GuildDuelState {
    std::int32_t status=0,stage=0;
    std::uint32_t opponent_id=0,leader_id=0,participants=0;
    void* guild=nullptr;
};
}
