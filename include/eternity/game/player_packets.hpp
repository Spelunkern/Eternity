#pragma once
#include "eternity/game/players.hpp"

namespace eternity::game {
struct ExperienceBounds {std::uint32_t previous,next;};
class Experience {
public:
    static Experience load(const storage::ResultSet& definitions);
    ExperienceBounds bounds(std::uint8_t grow,std::uint16_t level) const;
private:
    std::array<std::array<std::uint32_t,61>,4> levels_{};
};
struct PlayerResources {std::uint32_t hp,mp,sp;};
// Max resources must come from game rules/equipment/effects, not persisted current HP/MP/SP.
net::Packet character_data(const PlayerCore& player,PlayerResources maximum,ExperienceBounds experience,std::string_view guild);
}
