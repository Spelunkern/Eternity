#include "eternity/net/framing.hpp"
#include <iostream>

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

int main() {
    using namespace eternity::net;
    try {
        // Wire length includes the header; A101 is the original login key opcode.
        const Packet payload = {0x01, 0xa1, 0x00};
        const Packet expected = {0x05, 0x00, 0x01, 0xa1, 0x00};
        require(frame(payload) == expected, "Original wire header changed");
        Packet stream = expected;
        const auto second = frame(Packet{0x02, 0xa1, 0x00, 0x01});
        stream.insert(stream.end(), second.begin(), second.end());
        for (std::size_t split = 0; split <= stream.size(); ++split) {
            FrameDecoder decoder;
            auto packets = decoder.feed(std::span(stream).first(split));
            const auto remaining = decoder.feed(std::span(stream).subspan(split));
            packets.insert(packets.end(), remaining.begin(), remaining.end());
            require(packets.size() == 2 && packets[0] == payload && packets[1] == Packet({0x02, 0xa1, 0x00, 0x01}),
                    "TCP fragmentation or coalescing corrupted packets");
            require(decoder.buffered() == 0, "Completed frames left buffered data");
        }
        FrameDecoder bytewise;
        std::vector<Packet> packets;
        for (const auto& byte : stream) {
            const auto next = bytewise.feed(std::span(&byte, 1));
            packets.insert(packets.end(), next.begin(), next.end());
        }
        require(packets.size() == 2, "Bytewise TCP input lost packets");
        const Packet largest(maximum_frame_size - 2, 0x5a);
        FrameDecoder boundary;
        require(boundary.feed(frame(largest)) == std::vector<Packet>{largest}, "Maximum-size frame failed");
        FrameDecoder empty;
        require(empty.feed(Packet{2, 0}) == std::vector<Packet>{Packet{}}, "Original minimum frame rejected");
        for (const Packet invalid : {Packet{0, 0}, Packet{1, 0}, Packet{1, 8}, Packet{255, 255}}) {
            FrameDecoder decoder;
            bool rejected = false;
            try { (void)decoder.feed(invalid); } catch (const std::invalid_argument&) { rejected = true; }
            require(rejected && decoder.failed(), "Invalid wire length accepted");
            rejected = false;
            try { (void)decoder.feed(expected); } catch (const std::logic_error&) { rejected = true; }
            require(rejected, "Invalid connection was silently reused");
        }
        // A malformed trailing header must not erase previously completed frames.
        {
            FrameDecoder decoder;Packet batch=expected;batch.insert(batch.end(),{0,0});std::vector<Packet> delivered;bool invalid=false;
            try {decoder.feed_each(batch,[&](Packet packet){delivered.push_back(std::move(packet));return true;});}
            catch(const std::invalid_argument&) {invalid=true;}
            require(invalid && decoder.failed() && delivered==std::vector<Packet>{payload},"Malformed tail suppressed a complete frame");
        }
        {
            FrameDecoder decoder;Packet batch=expected;batch.insert(batch.end(),{0,0});unsigned delivered=0;
            decoder.feed_each(batch,[&](Packet packet){++delivered;return packet!=payload;});
            require(delivered==1 && !decoder.failed(),"Closed consumer parsed trailing bytes");
        }
        bool rejected = false;
        try { (void)frame(Packet(maximum_frame_size - 1)); } catch (const std::invalid_argument&) { rejected = true; }
        require(rejected, "Oversize outgoing packet accepted");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
