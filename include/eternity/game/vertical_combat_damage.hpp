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
// Borrow raw stSkillInfo +66 and +68/+6a/+6c from the same retained skill
// definition. attack_recovery is the live HP/SP/MP owner for CUser +1238/3c/40;
// it is required only by mode 3 when at least one damage word is nonzero.
struct VerticalCombatSkillDamageInputs {
    const UserSkillDefinition& skill;
    std::uint8_t mode;
    std::array<std::uint16_t,3> values; // HP, MP, SP
    std::span<const std::int32_t> attack_recovery; // HP, SP, MP
};
// Skill AttackDamage 0044d540 modes 0..5 and other-mode fallback. Melee/ranged
// weapon branches are supported; magic weapon branches fail explicitly.
// No hit, death, item option or network effects are supplied here.
bool vertical_combat_skill_damage(Actor&,MobRuntime&,unsigned kind,RandomSource&,
    ZoneUsers&,VerticalCombatRangedPassive&,std::span<const float,81>,
    const VerticalCombatSkillDamageInputs&,std::array<std::int32_t,3>&);
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
