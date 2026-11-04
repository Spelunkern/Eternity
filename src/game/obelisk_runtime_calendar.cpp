#include "eternity/game/obelisk_runtime_calendar.hpp"
#include <cmath>
#include <limits>

namespace eternity::game {
std::uint32_t pack_obelisk_calendar(const ObeliskRuntimeCalendar& calendar) noexcept {
    auto value=static_cast<std::uint32_t>(calendar.year)-16U;
    value=(value<<4)+calendar.month;value=(value<<5)+calendar.day;
    value=(value<<5)+calendar.hour;value=(value<<6)+calendar.minute;
    return (value<<6)+calendar.second;
}
ObeliskRuntimeCalendar unpack_obelisk_calendar(std::uint32_t value) noexcept {
    return {static_cast<std::uint16_t>(2000U+(value>>26)),static_cast<std::uint16_t>((value>>22)&15U),0,
        static_cast<std::uint16_t>((value>>17)&31U),static_cast<std::uint16_t>((value>>12)&31U),
        static_cast<std::uint16_t>((value>>6)&63U),static_cast<std::uint16_t>(value&63U),0};
}
std::uint32_t ObeliskRuntimeCalendarClock::packed_now() {return pack_obelisk_calendar(platform_.local_calendar());}
std::uint32_t ObeliskRuntimeCalendarClock::seconds_between(std::uint32_t now,std::uint32_t change) {
    double before=0,after=0;
    static_cast<void>(platform_.variant_day(unpack_obelisk_calendar(change),before));
    static_cast<void>(platform_.variant_day(unpack_obelisk_calendar(now),after));
    // Original spills the subtraction to binary64, then multiplies with the
    // CRT's default 53-bit precision (_setdefaultprecision 0051136e), truncates
    // to signed 64 bits, and returns only the low 32 bits. Do not round seconds.
    const volatile double difference=after-before;
    const volatile double seconds=difference*86400.0;
    const auto integral=std::trunc(seconds);
    constexpr auto limit=9223372036854775808.0; // 2^63, exactly representable.
    if(!std::isfinite(integral) || integral < -limit || integral>=limit) return 0;
    return static_cast<std::uint32_t>(static_cast<std::int64_t>(integral));
}
}
