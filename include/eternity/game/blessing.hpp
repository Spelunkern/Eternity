#pragma once
#include "eternity/game/skills.hpp"
#include <span>

namespace eternity::game {
struct BlessingDefinition {
    std::uint32_t index=0,kill_count=0;
    std::int32_t kill_value=0;
    std::array<std::int32_t,28> modifiers{};
};
class BlessingDefinitions {
public:
    static BlessingDefinitions decode(std::span<const std::uint8_t> data);
    const BlessingDefinition* find(std::uint8_t country,std::int32_t index) const;
    const BlessingDefinition* by_count(std::uint8_t country,std::uint32_t kills) const;
    std::size_t size(std::uint8_t country) const;
private:
    std::array<std::vector<BlessingDefinition>,2> countries_;
};
// Initialization only. Already-calculated defenses precede blessing in the original actor.
// Returned applied modifiers must be retained for later UpdateKCStatus reversal.
std::array<std::int32_t,28> initialize_blessing(SkillStatus& status,std::array<std::int32_t,3>& attack_recovery,
    const BlessingDefinition& definition,const std::array<std::int32_t,2>& calculated_defenses);
net::Packet blessing_packet(std::uint8_t country,std::uint32_t kill_count,std::uint32_t end_tick,std::uint32_t tick);
}
