#pragma once
#include "eternity/game/vertical_combat_damage.hpp"
namespace eternity::game {
// An adapter for the captured UserSkillDefinition passed to the actual resolved
// skill dispatch. It uses the same runtime's Actor and the supplied retained
// mob/zone/RNG/definition owners. It is the calculation stage only: callers
// must execute the original AttackMob 0044fd90 prefix and remaining hit/death,
// packets, secondary damage and item effects in order.
class VerticalCombatSkillRuntimeDamage {
public:
    VerticalCombatSkillRuntimeDamage(UserSkillRuntime&,RandomSource&,ZoneUsers&,
        VerticalCombatRangedPassive&,std::span<const float,81>);
    RandomSource& random_source() const noexcept {return random_;}
    bool bound_to(const UserSkillRuntime& runtime) const noexcept {return &owner_==&runtime;}
    bool calculate(Actor&,MobRuntime&,const UserSkillDefinition& captured,
        std::uint8_t raw_mode,std::array<std::uint16_t,3> raw_values,
        std::array<std::int32_t,3>& damage);
private:
    UserSkillRuntime& owner_;RandomSource& random_;ZoneUsers& zone_;
    VerticalCombatRangedPassive& passive_;std::span<const float,81> elements_;
};
}
