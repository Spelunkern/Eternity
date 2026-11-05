#pragma once
#include "eternity/game/vertical_combat_hit.hpp"
namespace eternity::game {
// Required real owners for CMob::Damage 00490ee0. Pure accessors must read
// the same retained mob/definition, without callbacks or cached projections.
// The live mob target_kind owns CMob +128. Events, clock,
// logging and retarget must run their real owners; actor, mob, ledger, damage
// triple, definition and FSM remain retained and serialized throughout.
class VerticalCombatLiveDamageServices:public VerticalCombatHitServices {
public:
 bool damage(Actor&,MobRuntime&,std::array<std::int32_t,3>&,const MobCombatCredit&) final;
 virtual bool long_corpse_delay(const MobRuntime&)=0;
 virtual bool damage_logging_enabled(const MobRuntime&)=0;
 virtual void dispatch_death_event(MobRuntime&)=0;
 virtual std::uint32_t tick_ms()=0;
 virtual void log_damage(MobRuntime&,std::uint32_t)=0;
 virtual void retarget(MobRuntime&,bool stacked)=0;
};
}
