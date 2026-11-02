#pragma once
#include "eternity/net/framing.hpp"
#include <array>
#include <optional>

namespace eternity::game {
struct GuildNpcLevel {std::uint16_t type=0,level=0;};
struct GuildDbNpcLevel:GuildNpcLevel {std::uint16_t marker=0;};
class GuildNpcReplica {
public:
    std::array<GuildDbNpcLevel,8> slots{};
    // DBGuild::LoadNpcLvList 00427f40: valid slot overwrites only its row.
    void load(std::uint8_t slot,std::uint16_t type,std::uint8_t level);
    bool upgrade(std::uint8_t slot,std::uint16_t type,std::uint8_t level);
    void reset();
    // GuildInfo::SendGuildNpcLv, ps_dbAgent.exe 0040f660, 0b0e.
    std::optional<net::Packet> packet(std::uint32_t guild) const;
};
class GuildNpcs {
public:
    std::array<GuildNpcLevel,8> slots{};
    // PacketGuild 00408e00, 0b0e branch: caller resolves the guild ID first.
    // Invalid slots are rejected instead of reproducing an out-of-bounds write.
    bool apply(std::span<const std::uint8_t>);
    // CGuild::SendNpcLvList, ps_game.exe 0042f0e0, 0d24. Empty lists omitted.
    std::optional<net::Packet> client_packet() const;
};
}
