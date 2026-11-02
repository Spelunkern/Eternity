#pragma once
#include "eternity/game/players.hpp"
#include "eternity/game/zone_users.hpp"
#include <span>

namespace eternity::game {
struct GameLogSubject {
    std::uint32_t account_id=0,character_id=0,experience=0;
    std::string account_name,character_name; // Original bytes, at most 18/20 bytes.
    std::uint16_t level=0,map=0;
    UserPosition position;
};
// SetGameLogMain 0045b0d0 fills fields within the 74-byte header, preserving string tails.
// The calendar represents local wall time, matching the original GetLocalTime call.
void write_game_log_main(std::span<std::uint8_t>,const GameLogSubject&,std::chrono::sys_seconds local_calendar);
}
