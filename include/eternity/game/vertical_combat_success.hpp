#pragma once
#include "eternity/game/vertical_combat.hpp"
#include "eternity/game/random.hpp"
#include "eternity/game/user_skill_runtime.hpp"
#include <span>
namespace eternity::game {
// Null-skill melee/ranged/magic branches of ps_game.exe CUser::AttackSuccess 0044d010.
// Uses the retained actor and mob stats and the same serialized MSVCRT RNG owner.
// Other kinds return true after the common guards without drawing.
bool vertical_combat_normal_success(const Actor&,const MobRuntime&,unsigned kind,RandomSource&);
// Borrow the original mob definition immunity bytes and skill hit bytes from
// their authoritative loader. The first state byte corresponds to state 1.
struct VerticalCombatSkillSuccessInputs {
    const UserSkillDefinition& skill;
    std::span<const std::uint8_t> state_immunity;
    std::span<const std::uint8_t> skill_immunity; // exactly six bytes
    std::uint8_t hit_type,hit_value; // stSkillInfo +54/+55
};
bool vertical_combat_skill_success(const Actor&,const MobRuntime&,unsigned kind,
    const VerticalCombatSkillSuccessInputs&,RandomSource&);
// Supplies the normal caller's required success provider without a second state owner.
class VerticalCombatNormalSuccessWorld:public VerticalCombatWorld {
public:
    explicit VerticalCombatNormalSuccessWorld(RandomSource& random):random_(random){}
    bool attack_success(Actor&,MobRuntime&,unsigned kind) final;
private: RandomSource& random_;
};
}
