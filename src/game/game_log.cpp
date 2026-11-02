#include "eternity/game/game_log.hpp"
#include <bit>
#include <algorithm>

namespace eternity::game {
void write_game_log_main(std::span<std::uint8_t> packet,const GameLogSubject& subject,std::chrono::sys_seconds calendar) {
    // ps_game.exe CUser::SetGameLogMain 0045b0d0; GetSystemTime 004cb8e0 calls GetLocalTime.
    if(packet.size()<74 || subject.account_name.size()>18 || subject.character_name.size()>20 ||
        subject.account_name.find('\0')!=std::string::npos || subject.character_name.find('\0')!=std::string::npos)
        throw std::invalid_argument("Invalid original game log header");
    const auto put=[&](std::size_t offset,std::uint32_t value,std::size_t width) {for(std::size_t i=0;i<width;++i) packet[offset+i]=static_cast<std::uint8_t>(value>>(8*i));};
    const auto name=[&](std::size_t offset,const std::string& value) {std::copy(value.begin(),value.end(),packet.begin()+static_cast<std::ptrdiff_t>(offset));packet[offset+value.size()]=0;};
    put(2,subject.account_id,4);put(25,subject.character_id,4);put(50,subject.level,2);put(52,subject.experience,4);put(56,subject.map,2);
    put(58,std::bit_cast<std::uint32_t>(subject.position.x),4);put(62,std::bit_cast<std::uint32_t>(subject.position.y),4);put(66,std::bit_cast<std::uint32_t>(subject.position.z),4);
    const auto day=std::chrono::floor<std::chrono::days>(calendar);const std::chrono::year_month_day date{day};const std::chrono::hh_mm_ss clock{calendar-day};
    // Preserve the original DWORD calendar wrap, including years beyond 2063.
    const auto stamp=((static_cast<std::uint32_t>(static_cast<int>(date.year()))-2000U)<<26) |
        (static_cast<unsigned int>(date.month())<<22) | (static_cast<unsigned int>(date.day())<<17) |
        (static_cast<std::uint32_t>(clock.hours().count())<<12) | (static_cast<std::uint32_t>(clock.minutes().count())<<6) | static_cast<std::uint32_t>(clock.seconds().count());
    put(70,stamp,4);name(6,subject.account_name);name(29,subject.character_name);
}
}
