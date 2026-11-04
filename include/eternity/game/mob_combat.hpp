#pragma once
#include <array>
#include <cstdint>
#include "eternity/game/mob_damage.hpp"
namespace eternity::game {
// Explicit projection of CMob fields; ps_game.exe Damage 00490cf0/00490ee0.
struct MobCombatState {
    std::uint32_t hp=0, hit_count=0, death_tick=0, secondary_state=0;
    std::uint16_t mp=0,sp=0;
    std::int32_t state=0;
    bool count_hits=false, stacked_fsm=false, long_corpse_delay=false;
    bool log_enabled=false, linked_log_enabled=false;
};
// Synchronous providers may update the projection. Event 3 observes zero HP
// before this hit enters the ledger; later stages use the resulting live state.
class MobCombatServices {
public:
    virtual ~MobCombatServices()=default;
    virtual std::int32_t status(const MobCombatState&)=0;
    virtual void dispatch_death_event(MobCombatState&)=0; // event 3
    virtual std::uint32_t tick()=0;
    virtual void log_damage(MobCombatState&,std::uint32_t)=0; // log type 2
    virtual void retarget(MobCombatState&,bool stacked)=0;
};
struct MobCombatCredit {
    std::int32_t type=1;
    std::uint32_t id=0,party=0;
    std::int32_t bonus=0,base_aggro=0;
};
bool mob_combat_damage(MobCombatState&,MobDamageLedger&,MobCombatServices&,std::uint32_t,const MobCombatCredit&);
// The triple remains live across callbacks (the original reloads its values).
bool mob_combat_damage(MobCombatState&,MobDamageLedger&,MobCombatServices&,const std::array<std::int32_t,3>&,const MobCombatCredit&);
// Both DamageParty overloads (00490e40/00490e90) use only first pointed value.
bool mob_combat_party(MobCombatState&,MobDamageLedger&,MobCombatServices&,std::uint32_t party,std::int32_t damage);
}
