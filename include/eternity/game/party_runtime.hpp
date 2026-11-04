#pragma once
#include <cstdint>
#include <optional>
#include <span>

namespace eternity::game {
// Live projection: original CParty +0x114, +0x10c, and +0x18/8-byte slots.
struct PartyRuntime {
    std::uint8_t kind{};
    std::uint32_t experience_level{};
    // ps_game.exe CParty::ChkPartyExpByLevel 00444d80. Normal slots must exist;
    // raid slots can be empty. Throws for an invalid normal-party projection.
    void check_experience_level(std::span<const std::optional<std::uint16_t>> members);
    // ps_game.exe CParty::GetPartyExpByLevel 00444e40, signed wrapped subtraction.
    double experience_factor(std::int32_t member_level) const noexcept;
};

struct PartyRewardUser {
    std::int32_t life_status{};       // CUser +0x142c
    std::uint32_t reward_state{};     // CUser +0x5758; semantic owner unresolved
    std::uint32_t zone_identity{};    // +0xe0, pointer identity in original
    std::uint16_t instance{};         // +0x164
    float x{}, y{}, z{};              // +0xd0
};
// ps_game.exe CUser::EnableApplyRange 0045de60. Candidate is original ECX,
// reference is original EAX; only candidate life/reward status is checked.
bool party_reward_in_range(const PartyRewardUser& candidate,
                           const PartyRewardUser& reference, std::int32_t range) noexcept;
}
