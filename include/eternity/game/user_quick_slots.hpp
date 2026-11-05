#pragma once
#include "eternity/game/players.hpp"
#include "eternity/net/framing.hpp"
#include <functional>

namespace eternity::game {
enum class QuickSlotUpdate {unhandled,accepted,closed};
// Retained CUser+ec4/ec8 state; slots past the live count preserve old values.
class UserQuickSlots {
public:
    explicit UserQuickSlots(std::span<const QuickSlot> initial={}) {load(initial);}
    void load(std::span<const QuickSlot> initial) {
        if(initial.size()>slots_.size())throw std::invalid_argument("Quick-slot load exceeds retained capacity");
        std::copy(initial.begin(),initial.end(),slots_.begin());count_=static_cast<std::uint32_t>(initial.size());
    }
    std::span<const QuickSlot> rows() const noexcept {return {slots_.data(),count_};}
    // PacketPC 010b 00467058. A bag 255 row is skipped when the prior row's
    // bar byte is <10, but destination still advances; count tracks accepted
    // rows rather than compacting them. Sends the same DB snapshot immediately.
    QuickSlotUpdate receive(std::span<const std::uint8_t> packet,std::uint32_t account,
        const std::function<void(const net::Packet&)>& send,const std::function<void(std::uint32_t)>& close) {
        if(packet.size()<2 || packet[0]!=0x0b || packet[1]!=1)return QuickSlotUpdate::unhandled;
        if(packet.size()<3)throw std::invalid_argument("Truncated quick-slot request");
        if(!send || !close)throw std::invalid_argument("Quick slots require delivery and close services");
        const auto count=packet[2];if(count>=128){close(9);return QuickSlotUpdate::closed;}
        if(packet.size()<3U+5U*count)throw std::invalid_argument("Truncated quick-slot request");
        count_=0;
        for(unsigned i=0;i<count;++i) {
            const auto at=3U+5U*i;if(packet[at+2]==255 && slots_[i].bar<10)continue;
            slots_[i]={packet[at],packet[at+1],packet[at+2],static_cast<std::uint16_t>(packet[at+3]|(unsigned(packet[at+4])<<8))};++count_;
        }
        save(account,send);return QuickSlotUpdate::accepted;
    }
    // SendDBAgentCharGetInfo 0046ce4a and 0046c900. Empty means no update.
    void save(std::uint32_t account,const std::function<void(const net::Packet&)>& send) const {
        if(!send)throw std::invalid_argument("Quick-slot save requires delivery");if(!count_)return;
        net::Packet packet{9,6};for(unsigned i=0;i<4;++i)packet.push_back(static_cast<std::uint8_t>(account>>(8*i)));packet.push_back(static_cast<std::uint8_t>(count_));
        for(const auto& row:rows()){packet.insert(packet.end(),{row.bar,row.slot,row.bag,static_cast<std::uint8_t>(row.number),static_cast<std::uint8_t>(row.number>>8)});}
        send(packet);
    }
private:
    std::array<QuickSlot,128> slots_{};std::uint32_t count_=0;
};
}
