#pragma once
#include <chrono>
#include <cstdint>
#include <mutex>

namespace eternity::game {
struct CreationTime {std::uint64_t ticks_ms;std::chrono::sys_seconds local_calendar;};
CreationTime creation_time();
// STime::GetSystemTime 004cb8e0: local calendar arithmetic modulo DWORD,
// including years outside pack_world_time's database-world range.
std::uint32_t pack_item_id_time(std::chrono::sys_seconds);
class ItemIdClock {
public:
    virtual ~ItemIdClock()=default;
    virtual std::uint32_t ticks_ms()=0;
    virtual std::uint32_t packed_local_calendar()=0;
};
// One retained instance per original g_UniqueIDGen (00547ce0), shared by all
// item-producing services. Constructor samples local calendar, not tick count.
// The clock must outlive this object; use the injected owner for runtime calls.
class ItemIds {
public:
    explicit ItemIds(std::uint8_t server);
    ItemIds(std::uint8_t server,ItemIdClock&);
    std::uint64_t next();
    std::uint64_t raw_next(); // GenID, before the caller adds server byte once.
    // Compatibility adapters for existing snapshot-clock callers. Sampling has
    // already happened outside the lock; prefer next()/raw_next() for runtime.
    std::uint64_t next(CreationTime now);
    std::uint64_t raw_next(CreationTime now);
    // SetStartID 004494d0 changes only the next-refresh baseline, not current ID.
    void set_start_id(std::uint32_t);
private:
    std::uint64_t generate(std::uint32_t tick,ItemIdClock&);
    std::recursive_mutex mutex_;
    ItemIdClock& clock_;
    std::uint8_t server_;
    std::uint32_t counter_=0,start_=0,update_at_=0,stamp_;
};
}
