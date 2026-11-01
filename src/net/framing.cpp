#include "eternity/net/framing.hpp"
#include <algorithm>

namespace eternity::net {
std::vector<Packet> FrameDecoder::feed(std::span<const std::uint8_t> bytes) {
    if (failed_) throw std::logic_error("Transport decoder is closed after an invalid frame");
    std::vector<Packet> packets;
    while (!bytes.empty()) {
        const auto count = std::min(expected_ - used_, bytes.size());
        std::copy_n(bytes.begin(), count, buffer_.begin() + static_cast<std::ptrdiff_t>(used_));
        used_ += count;
        bytes = bytes.subspan(count);
        if (used_ < expected_) continue;
        if (expected_ == 2) {
            expected_ = static_cast<std::size_t>(buffer_[0]) |
                        (static_cast<std::size_t>(buffer_[1]) << 8);
            if (expected_ < 2 || expected_ > maximum_frame_size) {
                failed_ = true;
                throw std::invalid_argument("Original transport frame length must be in 2..2048");
            }
            if (used_ < expected_) continue;
        }
        packets.emplace_back(buffer_.begin() + 2,
                             buffer_.begin() + static_cast<std::ptrdiff_t>(expected_));
        used_ = 0;
        expected_ = 2;
    }
    return packets;
}

Packet frame(std::span<const std::uint8_t> payload) {
    if (payload.size() > maximum_frame_size - 2)
        throw std::invalid_argument("Payload exceeds original transport receive limit");
    const auto size = payload.size() + 2;
    Packet output;
    output.reserve(size);
    output.push_back(static_cast<std::uint8_t>(size & 0xff));
    output.push_back(static_cast<std::uint8_t>(size >> 8));
    output.insert(output.end(), payload.begin(), payload.end());
    return output;
}
}
