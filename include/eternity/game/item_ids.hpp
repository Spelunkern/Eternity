#pragma once
#include <chrono>
#include <cstdint>
#include <mutex>

namespace eternity::game {
struct CreationTime {std::uint64_t ticks_ms;std::chrono::sys_seconds local_calendar;};
CreationTime creation_time();

class ItemIds {
public:
    explicit ItemIds(std::uint8_t server) : server_(server) {}
    std::uint64_t next(CreationTime now);
private:
    std::mutex mutex_;
    std::uint8_t server_;
    std::uint32_t counter_ = 0,stamp_ = 0;
    std::uint64_t update_at_ = 0;
};
}
