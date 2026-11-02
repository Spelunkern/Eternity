#pragma once
#include "eternity/game/definitions.hpp"
#include "eternity/game/player_packets.hpp"

namespace eternity::game {
struct Attributes {
    std::int32_t strength = 0,dexterity = 0,recovery = 0,intelligence = 0,wisdom = 0,luck = 0;
};
struct BaseStatus { Attributes attributes; PlayerResources maximum; };
class StatusRules {
public:
    static StatusRules load(const storage::ResultSet& definitions);
    // Initial phase only: equipment, faction blessing, passives, effects and billing follow this.
    BaseStatus base(const CharacterSummary& character) const;
    // CUser::LevelUpStatus 0048a5e0: mutate persisted and live base state.
    void level_up(PlayerCore&,BaseStatus&) const;
private:
    std::array<std::array<PlayerResources,61>,6> levels_{};
};
struct AttackAdd {
    std::int32_t accuracy = 0,attack = 0,evasion = 0,defense = 0;
};
struct AttackModifiers {
    std::array<AttackAdd,3> categories{};
    std::int32_t item_attack = 0,physical_defense = 0,magic_defense = 0,critical = 0;
};
struct AttackValues {
    std::int32_t accuracy,evasion,attack,defense,critical_attack,critical_defense;
};
// CUser::SetAttack: melee, ranged and magic. Does not resolve attack damage or random rolls.
std::array<AttackValues,3> calculate_attack(std::uint16_t level,const Attributes& attributes,const AttackModifiers& modifiers);
}
