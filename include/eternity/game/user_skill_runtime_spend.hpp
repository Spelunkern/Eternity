#pragma once
#include "eternity/game/user_skill_runtime_cost.hpp"
namespace eternity::game {
// Pure recovered resource transition ONLY, not SendUseSPMP transport/party owner.
// Inputs are actual live DWORD resources; costs are the resolved skill WORDs.
struct UserSkillSpendTransition {
    std::uint32_t sp,mp;
    std::int32_t requested_sp,requested_mp; // Party notification guards, not actual deltas.
    net::Packet self_packet;
};
UserSkillSpendTransition user_skill_runtime_spend_transition(std::uint32_t sp,
    std::uint32_t mp,std::uint16_t sp_cost,std::uint16_t mp_cost,
    std::int32_t sp_reduction,std::int32_t mp_reduction);
}
