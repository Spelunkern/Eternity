#pragma once
#include "eternity/game/vertical_combat_success.hpp"
#include "eternity/game/zone_users.hpp"
namespace eternity::game {
class VerticalCombatRangedPassive {
public:
    virtual ~VerticalCombatRangedPassive()=default;
    // Pure, non-reentrant read of actual CUser +1330. Its owner is not yet exposed.
    virtual std::int32_t ranged_penalty_suppression(const Actor&)=0;
};
// Retain the actor, mob, mob definition, RNG, element table and zone through
// the complete synchronous call. The zone must own the same actor position;
// the table is the actual 9x9 CGameData::m_AttackAttr owner, not a guessed table.
// Only normal melee/ranged calls (null skill) are supported. A missing retained
// ranged position or negative matrix index is rejected where it is needed.
bool vertical_combat_normal_damage(Actor&,MobRuntime&,unsigned kind,RandomSource&,
    ZoneUsers&,VerticalCombatRangedPassive&,std::span<const float,81> elements,std::array<std::int32_t,3>& damage);
// Supplies AttackSuccess and AttackDamage to the existing caller using the same
// serialized CRT random stream. Remaining world services stay mandatory.
class VerticalCombatNormalDamageWorld:public VerticalCombatNormalSuccessWorld {
public:
    VerticalCombatNormalDamageWorld(RandomSource& random,ZoneUsers& zone,VerticalCombatRangedPassive& passive,std::span<const float,81> elements)
      :VerticalCombatNormalSuccessWorld(random),random_(random),zone_(zone),passive_(passive),elements_(elements){}
    bool attack_damage(Actor&,MobRuntime&,unsigned,std::array<std::int32_t,3>&) final;
private:
    RandomSource& random_;ZoneUsers& zone_;VerticalCombatRangedPassive& passive_;std::span<const float,81> elements_;
};
}
