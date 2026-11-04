#pragma once
#include "eternity/game/mob_combat.hpp"
#include <optional>
namespace eternity::game {
struct MobCombatAttacker {
    std::uint32_t id=0;
    std::int32_t aggro_bonus=0; // CUser +1230, multiplied by ten
    // Presence projects CUser +173c; a present party can have identity zero.
    std::optional<std::uint32_t> party;
};
class MobCombatAttackServices:public MobCombatServices {
public:
    virtual void stop_attack(MobCombatAttacker&)=0; // CUser::StatusAttackStop 00489ed0
    virtual void death(MobCombatState&,std::int32_t type,std::uint32_t id)=0; // 004910f0
};
// Recovered user-to-mob hit sequence in CUser::AttackMob 0044ce10/0044fd90.
// Requires a valid mob; damage calculation, secondary-hit guards and packets
// belong to the caller. party_damage is explicit: secondary skill hits at
// 004501ce/00450489 credit the preceding primary HP damage to the party.
// Services and projections retain their storage throughout synchronous calls.
bool mob_combat_user_hit(MobCombatState&,MobDamageLedger&,MobCombatAttackServices&,
                         MobCombatAttacker&,const std::array<std::int32_t,3>& damage,
                         std::int32_t party_damage,std::int32_t base_aggro);
}
