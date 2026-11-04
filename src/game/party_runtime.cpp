#include "eternity/game/party_runtime.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <stdexcept>

namespace eternity::game {
void PartyRuntime::check_experience_level(std::span<const std::optional<std::uint16_t>> members) {
    if (kind > 1 || members.size() <= 1) return;
    std::uint32_t maximum = 0;
    for (const auto level : members) {
        if (!level && kind == 0) throw std::invalid_argument("Normal party member is absent");
        if (level) maximum = std::max(maximum, static_cast<std::uint32_t>(*level));
    }
    experience_level = maximum;
}
double PartyRuntime::experience_factor(std::int32_t member_level) const noexcept {
    const auto difference = std::bit_cast<std::int32_t>(experience_level - static_cast<std::uint32_t>(member_level));
    if (difference < 8) return 1.0;
    if (difference < 16) return 0.8;
    if (difference < 32) return 0.6;
    if (difference < 48) return 0.4;
    return 0.2;
}
}

namespace eternity::game {
bool party_reward_in_range(const PartyRewardUser& candidate, const PartyRewardUser& reference, std::int32_t range) noexcept {
    if (candidate.life_status == 1 || candidate.reward_state != 4 ||
        candidate.zone_identity != reference.zone_identity || candidate.instance != reference.instance) return false;
    const float dx = candidate.x - reference.x;
    const float dz = candidate.z - reference.z;
    // Distance2 stores differences as float, accumulates x87 products before
    // storing a double for sqrt, then rounds the result to float.
    const double squared = static_cast<double>(dx) * dx + static_cast<double>(dz) * dz;
    const float distance = static_cast<float>(std::sqrt(squared));
    return !(distance > static_cast<float>(range)); // Original unordered FCOMP succeeds.
}
}
