#pragma once
#include "eternity/game/vertical_combat_damage.hpp"
namespace eternity::game {
// CUser +1330 is the existing SkillStatus::cross_road passive owner.
// The actor and its passive state remain retained and serialized through calls.
class VerticalCombatActorRangedPassive final:public VerticalCombatRangedPassive {
public:
 explicit VerticalCombatActorRangedPassive(const Actor& actor):actor_(actor){}
 std::int32_t ranged_penalty_suppression(const Actor&) final;
private:const Actor& actor_;
};
}
