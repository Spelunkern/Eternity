#pragma once
#include "eternity/game/quest_start.hpp"
#include <string_view>
namespace eternity::game {
// GetSystemTime 004cb8e0 packs local calendar fields, with 32-bit wrapping.
std::uint32_t quest_start_log_time(std::uint16_t year,std::uint16_t month,
    std::uint16_t day,std::uint16_t hour,std::uint16_t minute,std::uint16_t second);
// SetGameLogMain 0045b0d0: caller supplies the original system-time value,
// not the monotonic GetTickCount used by quest admission.
struct QuestStartLogContext {
    std::uint32_t user_id=0,character_id=0,experience=0,system_time=0;
    std::uint16_t level=0,map=0;
    std::array<std::uint32_t,3> position_bits{}; // Raw IEEE-754 bits, without conversion.
    std::string_view account_name,character_name;
};
struct QuestStartLogNpc {std::uint32_t type=0;std::string_view name;};
// QuestStart 0047ea70 writes only through each C-string's first terminator.
// Initialized storage preserves original retained tails; malformed oversized
// strings are rejected before any writes instead of reproducing memory damage.
void write_quest_start_log(std::span<std::uint8_t,144>,const QuestStartLogContext&,
    const QuestStartRecord&,const std::optional<QuestStartLogNpc>&);
// Deterministic fresh storage for production adapters. Capture this packet in
// prepare_start_log, before send_self; deliver it in log_start via required transport.
std::array<std::uint8_t,144> quest_start_log_packet(const QuestStartLogContext&,
    const QuestStartRecord&,const std::optional<QuestStartLogNpc>&);
}
