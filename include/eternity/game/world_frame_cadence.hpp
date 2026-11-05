#pragma once
#include <cstdint>

namespace eternity::game {
struct WorldFrameCadence {
    std::uint32_t wait_ms;
    bool overrun;
};
// ps_game.exe CWorldThread::Run 00407330, 0040734f..00407380.
// All ticks are GetTickCount DWORDs. The source reads the clock separately
// after Update and for overrun accounting; preserve both readings and wrap.
constexpr WorldFrameCadence world_frame_cadence(std::uint32_t start,
    std::uint32_t after_update, std::uint32_t overrun_tick) noexcept {
    const std::uint32_t remaining=start-after_update+250U;
    return {(remaining & 0x80000000U) ? 0U : remaining,
            overrun_tick-start>250U};
}
}
