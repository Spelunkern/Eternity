#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>

namespace eternity::net {
using Packet = std::vector<std::uint8_t>;
inline constexpr std::size_t maximum_frame_size = 2048;

// ps_login.exe SWorker::IORecv, 0x00416d60: little-endian total length, 2..2048.
// Payload includes its opcode; the two-byte transport length is not encrypted.
class FrameDecoder {
public:
    std::vector<Packet> feed(std::span<const std::uint8_t> bytes);
    bool failed() const noexcept { return failed_; }
    std::size_t buffered() const noexcept { return used_; }
private:
    std::array<std::uint8_t, maximum_frame_size> buffer_{};
    std::size_t used_ = 0;
    std::size_t expected_ = 2;
    bool failed_ = false;
};

Packet frame(std::span<const std::uint8_t> payload);
}
