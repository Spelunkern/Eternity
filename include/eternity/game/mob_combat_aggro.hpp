#pragma once
#include "eternity/game/mob_combat.hpp"
namespace eternity::game {
struct MobRuntime;
class MobCombatAggroServices {
public:
    virtual ~MobCombatAggroServices()=default;
    virtual std::int32_t status(const MobCombatState&)=0;
    virtual void retarget(MobCombatState&,bool stacked)=0;
};
// ps_game.exe CMob::Aggro 00491080. Use the existing mob-owned ledger; no
// identity lookup or target guard is present here. Required status/retarget
// providers project the live ordinary/stacked FSM and actual world targeting.
void mob_combat_aggro(MobCombatState&,MobDamageLedger&,MobCombatAggroServices&,
                      std::uint32_t user_id,std::int32_t amount);
class MobCombatAggroWorld {
public:
    virtual ~MobCombatAggroWorld()=default;
    virtual void retarget(MobRuntime&,bool stacked)=0;
};
// Uses MobRuntime's actual state_stack/status/HP and existing damage owner.
// Missing ownership is an invalid host state, never an implicit no-op. Owners
// must retain the subject and ledger for the complete synchronous invocation.
void mob_combat_aggro(MobRuntime&,MobCombatAggroWorld&,std::uint32_t user_id,std::int32_t amount);
}
