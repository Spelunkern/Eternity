#pragma once
#include <cstdint>
namespace eternity::game {
// Borrowed synchronous input projection, not another user/guild owner.
// ps_game.exe CUser::PvPGetDeathType 0047b270: EAX subject, ESI peer.
struct MobTargetingPvpRelation {
    std::uint32_t id;                 // +dc
    std::uint32_t duel_state;         // +54cc m_nPvPStatus, preserved raw bits
    std::uint32_t duel_peer;          // +54d4 m_dwPvPUser
    std::uint32_t guild;              // +1748 m_dwGuildID, not a guild pointer
    std::uint32_t opposing_guild;     // +54e4 m_dwPvPGuildID
};
enum class MobTargetingPvpDeathType : std::uint32_t { ordinary=3, duel=4, guild=5 };
// Identity is actual user identity, never equality of projected field values.
// Both views must refer to retained users under their existing serialization.
MobTargetingPvpDeathType mob_targeting_pvp_death_type(
    const MobTargetingPvpRelation& subject,const MobTargetingPvpRelation& peer,
    bool same_user) noexcept;
}
