#pragma once
#include <bit>
#include <cstdint>
#include <optional>

namespace eternity::net {
// ps_login.exe SReply::CheckRecv 00417d90 and CheckTime 00417d40,
// with accepted-agent parameters from CServer::Start 00401770.
// Socket owner must call observe_reply only for decoded opcode 0003, close
// on a returned reason, and schedule timeout checks on its serialized strand.
class LoginReplyClock {
public:
    std::optional<std::uint32_t> observe_reply(std::uint64_t now_ms) noexcept {
        const auto now=static_cast<std::uint32_t>(now_ms);
        const auto elapsed=std::bit_cast<std::int32_t>(now-last_);
        const auto reason=last_==0 ? 0U : elapsed<110000 ? 10U : elapsed>720000 ? 8U : 0U;
        last_=now;
        return reason ? std::optional<std::uint32_t>{reason} : std::nullopt;
    }
    // Fresh original SConnection initializes its +0x40 baseline to zero.
    bool timed_out(std::uint64_t now_ms) const noexcept {
        const auto now=static_cast<std::uint32_t>(now_ms);
        if(last_==0) return now>720000;
        return std::bit_cast<std::int32_t>(now-last_)>720000;
    }
private:
    std::uint32_t last_=0;
};
// SAgent::UpdateReply 00412e40 gates scans with its configured 120000 ms
// interval. SReply's worker wakes on a separate 300000 ms term.
class LoginReplyScanGate {
public:
    bool due(std::uint64_t now_ms) noexcept {
        const auto now=static_cast<std::uint32_t>(now_ms);
        if(now<next_) return false;
        next_=now+120000;return true;
    }
    std::uint32_t next() const noexcept {return next_;}
private:
    std::uint32_t next_=0;
};
}
