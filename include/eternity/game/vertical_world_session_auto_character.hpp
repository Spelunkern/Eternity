#pragma once
#include <cstdint>
#include <span>
#include <stdexcept>

namespace eternity::game {
// ps_game.exe CClientToMgr::OnRecv 00409850, branch 004099c1..00409a67 forwards
// TP_GUSER_CONNECT (0801) +5a to CWorld::LoginUser 0040c510, which stores
// CUser+57e8. This view reads no username, key, or billing fields.
class VerticalWorldSessionAutoCharacter final {
public:
    static VerticalWorldSessionAutoCharacter from_manager(std::span<const std::uint8_t> packet,std::uint32_t account,std::uint32_t connection) {
        // The original handler also reads the DWORD at +5f: its live variant
        // is 99 bytes even though one PDB variant omits that trailing field.
        if(packet.size()!=99 || packet[0]!=1 || packet[1]!=8 || packet[2]!=0)
            throw std::invalid_argument("Invalid successful manager connection packet");
        const auto word=[&](std::size_t offset){return std::uint32_t(packet[offset]) | (std::uint32_t(packet[offset+1])<<8) |
            (std::uint32_t(packet[offset+2])<<16) | (std::uint32_t(packet[offset+3])<<24);};
        if(!account || word(3)!=connection || word(7)!=account)
            throw std::invalid_argument("Manager connection packet has foreign ownership");
        return VerticalWorldSessionAutoCharacter(word(90));
    }
    std::uint32_t value() const noexcept {return value_;}
private:
    explicit VerticalWorldSessionAutoCharacter(std::uint32_t value):value_(value){}
    std::uint32_t value_;
};
}
