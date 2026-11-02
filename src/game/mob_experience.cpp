#include "eternity/game/mob_experience.hpp"
#include <bit>
#include <cmath>
namespace eternity::game {
namespace {
std::int32_t converted(double value) {
    const auto truncated=std::trunc(static_cast<long double>(value));
    // FISTP qword's invalid result has zero low DWORD; the caller keeps only it.
    if(!std::isfinite(truncated) || truncated<-9223372036854775808.0L || truncated>=9223372036854775808.0L) return 0;
    return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(static_cast<std::int64_t>(truncated)));
}
}
std::int32_t mob_experience(std::uint16_t level,std::uint16_t base,std::int32_t user_level,std::int32_t mode,std::int32_t active,double multiplier) {
    const auto difference=std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(level)-static_cast<std::uint32_t>(user_level));if(difference>9) return 0;
    const auto adjusted=std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(difference)+1U);
    volatile double scaled=static_cast<double>(adjusted)*0.2;volatile double factor=scaled+1.0;
    auto result=converted(static_cast<double>(base)*factor);
    if(mode==1 || (mode==2 && active==1)) result=converted(static_cast<double>(result)*multiplier);
    return result<0?0:result;
}
}
