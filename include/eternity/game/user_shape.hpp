#pragma once
#include "eternity/game/players.hpp"
#include <span>

namespace eternity::game {
struct UserShapeIdentity {
    std::uint8_t country=2,family=0,hair=0,face=0,size=0,job=0,sex=0,grow=0;
    std::uint32_t kill_count=0;
    std::string name; // Original wire bytes, at most 20 bytes.
};
struct UserShapeSocial {
    std::uint8_t party_role=0; // GetPartyType: 0 absent, 1 member, 2 leader.
    std::string guild_name; // Original wire bytes, at most 24 bytes.
};
// Snapshot at CUser+135c, captured by the original copied-user transformation.
// The first two list pointers are deliberately excluded.
struct CopiedUserShape {
    std::array<std::uint8_t,11> appearance{};
    std::uint32_t kill_count=0;
    std::array<std::uint8_t,24> equipment{};
    std::array<std::uint8_t,21> name{};
    std::array<std::uint8_t,31> extra{};
    std::uint8_t length=67;
};
UserShapeIdentity user_shape_identity(const PlayerCore&,std::uint8_t country);
net::Packet character_user_shape(std::uint32_t id,const UserShapeIdentity&,bool dead,std::uint8_t motion,
    const UserShapeSocial&,std::span<const PlayerItem> inventory);
net::Packet character_equipment(std::uint32_t id,std::uint8_t slot,const PlayerItem*);
net::Packet character_copied_user_shape(std::uint32_t id,const CopiedUserShape&);
}
