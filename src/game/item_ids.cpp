#include "eternity/game/item_ids.hpp"
#include "eternity/game/characters.hpp"
#include <ctime>
#include <limits>

namespace eternity::game {
CreationTime creation_time() {
    const auto now=std::chrono::system_clock::now();const auto raw=std::chrono::system_clock::to_time_t(now);
    std::tm local{};
    if(!localtime_r(&raw,&local)) throw std::runtime_error("Read local creation calendar failed");
    const auto date=std::chrono::year{1900+local.tm_year}/static_cast<unsigned int>(local.tm_mon+1)/static_cast<unsigned int>(local.tm_mday);
    return {static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count()),
        std::chrono::sys_days{date}+std::chrono::hours{local.tm_hour}+std::chrono::minutes{local.tm_min}+std::chrono::seconds{local.tm_sec}};
}
std::uint64_t ItemIds::next(CreationTime now) {
    // SUniqueIDGen::GenID 004cbe60 + CUser::ProcessCharacterCreate 00481750, ps_game.exe.
    std::lock_guard lock(mutex_);
    if(stamp_==0 || update_at_<=now.ticks_ms) {
        if(now.ticks_ms>std::numeric_limits<std::uint64_t>::max()-2000) throw std::overflow_error("Item identifier clock overflow");
        stamp_=pack_world_time(now.local_calendar);counter_=0;update_at_=now.ticks_ms+2000;
    }
    // The original shifts a 32-bit counter by 16; refuse its wrap rather than issue duplicate IDs.
    if(counter_==65535) throw std::overflow_error("Original item identifier interval exhausted");
    ++counter_;
    return (static_cast<std::uint64_t>(stamp_)<<32) | (static_cast<std::uint64_t>(counter_)<<16) | server_;
}
}
