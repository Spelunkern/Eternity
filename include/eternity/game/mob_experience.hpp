#pragma once
#include <cstdint>
namespace eternity::game {
// ps_game.exe CMob::GetExp 00491690. Inputs project the live definition and
// global event state; this does not award experience or substitute SQL loading.
std::int32_t mob_experience(std::uint16_t level,std::uint16_t base_experience,std::int32_t user_level,std::int32_t admin_mode,std::int32_t event_active,double multiplier);
}
