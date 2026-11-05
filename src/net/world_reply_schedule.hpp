#pragma once
#include <cstdint>

namespace eternity::net::detail {
// ps_game.exe SAgent::UpdateReply 004d9080: unsigned due comparison and
// wrapped 32-bit next tick. The caller supplies the agent's scan interval.
class WorldReplySchedule {
public:
    explicit WorldReplySchedule(std::uint32_t interval):interval_(interval) {}
    bool due(std::uint64_t now_ms) noexcept {
        const auto tick=static_cast<std::uint32_t>(now_ms);
        if(tick<next_) return false;
        next_=tick+interval_;return true;
    }
    std::uint32_t next() const noexcept {return next_;}
private:
    std::uint32_t interval_;
    std::uint32_t next_=0;
};
}
