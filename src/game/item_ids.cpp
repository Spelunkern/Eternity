#include "eternity/game/item_ids.hpp"
#include <ctime>
#include <stdexcept>

namespace eternity::game {
namespace {
std::chrono::sys_seconds local_calendar() {
    const auto raw=std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm local{};
    if(!localtime_r(&raw,&local)) throw std::runtime_error("Read local creation calendar failed");
    const auto date=std::chrono::year{1900+local.tm_year}/static_cast<unsigned int>(local.tm_mon+1)/static_cast<unsigned int>(local.tm_mday);
    return std::chrono::sys_days{date}+std::chrono::hours{local.tm_hour}+std::chrono::minutes{local.tm_min}+std::chrono::seconds{local.tm_sec};
}
std::uint32_t boot_ticks() {
    timespec time{};
    if(clock_gettime(CLOCK_BOOTTIME,&time)!=0) throw std::runtime_error("Read item identifier boot clock failed");
    return static_cast<std::uint32_t>(static_cast<std::uint64_t>(time.tv_sec)*1000U+static_cast<unsigned long>(time.tv_nsec)/1000000U);
}
class SystemClock final:public ItemIdClock {
public:
    std::uint32_t ticks_ms() override {return boot_ticks();}
    std::uint32_t packed_local_calendar() override {return pack_item_id_time(local_calendar());}
};
ItemIdClock& system_clock() {static SystemClock clock;return clock;}
class SnapshotClock final:public ItemIdClock {
public:
    explicit SnapshotClock(CreationTime time):time_(time) {}
    std::uint32_t ticks_ms() override {return static_cast<std::uint32_t>(time_.ticks_ms);}
    std::uint32_t packed_local_calendar() override {return pack_item_id_time(time_.local_calendar);}
private:
    CreationTime time_;
};
}
CreationTime creation_time() {return {boot_ticks(),local_calendar()};}
std::uint32_t pack_item_id_time(std::chrono::sys_seconds time) {
    const auto day=std::chrono::floor<std::chrono::days>(time);
    const std::chrono::year_month_day date{day};const std::chrono::hh_mm_ss clock{time-day};
    auto value=static_cast<std::uint32_t>(static_cast<std::uint16_t>(static_cast<int>(date.year())))-16U;
    value=value*16U+static_cast<unsigned int>(date.month());
    value=value*32U+static_cast<unsigned int>(date.day());
    value=value*32U+static_cast<std::uint32_t>(clock.hours().count());
    value=value*64U+static_cast<std::uint32_t>(clock.minutes().count());
    return value*64U+static_cast<std::uint32_t>(clock.seconds().count());
}
ItemIds::ItemIds(std::uint8_t server):ItemIds(server,system_clock()) {}
ItemIds::ItemIds(std::uint8_t server,ItemIdClock& clock):clock_(clock),server_(server),stamp_(clock.packed_local_calendar()) {}
std::uint64_t ItemIds::generate(std::uint32_t tick,ItemIdClock& clock) {
    // SUniqueIDGen::GenID 004cbe60; deliberately unsigned absolute comparison.
    // Wrap can refresh repeatedly before the boot tick itself wraps.
    if(tick>=update_at_) {update_at_=tick+2000U;counter_=start_;stamp_=clock.packed_local_calendar();}
    ++counter_;
    const auto low=counter_<<16;
    return (static_cast<std::uint64_t>(stamp_)<<32)|low;
}
std::uint64_t ItemIds::raw_next() {std::lock_guard lock(mutex_);return generate(clock_.ticks_ms(),clock_);}
std::uint64_t ItemIds::next() {return raw_next()+server_;}
std::uint64_t ItemIds::raw_next(CreationTime now) {std::lock_guard lock(mutex_);SnapshotClock clock(now);return generate(clock.ticks_ms(),clock);}
std::uint64_t ItemIds::next(CreationTime now) {return raw_next(now)+server_;}
void ItemIds::set_start_id(std::uint32_t value) {std::lock_guard lock(mutex_);start_=value;}
}
