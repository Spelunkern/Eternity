#pragma once
#include "eternity/game/vertical_combat_fsm.hpp"
#include "eternity/game/vertical_combat_mob_position.hpp"
namespace eternity::game {
class VerticalCombatClock {
public:
 virtual ~VerticalCombatClock()=default;
 virtual std::uint32_t tick_ms()=0; // actual shared GetTickCount
};
// The same live secondary state, definition and clock serve CMob::Damage and
// CMob::GetCurrentPos. The mob's retained target_kind owns CMob +128.
// mob_definition_record remains a pure required borrow from this exact mob's
// actual definition owner. The shared clock and all mob/zone/definition/FSM services
// survive synchronous callbacks, and the world executor serializes them.
class VerticalCombatMobStateServices:public VerticalCombatFsmHitServices,public VerticalCombatMobPositionServices {
public:
 VerticalCombatMobStateServices(Actor& actor,PartyMembershipUser& member,const MobDeathExperience& rules,MobDeathWorld& world,MobAiStateServices& fsm,VerticalCombatClock& clock)
  :VerticalCombatFsmHitServices(actor,member,rules,world,fsm),clock_(clock){}
 std::uint32_t tick_ms() final {return clock_.tick_ms();}
 std::uint32_t mob_secondary_state(const MobRuntime& mob) final {return static_cast<std::uint32_t>(mob.target_kind);}
 bool long_corpse_delay(const MobRuntime& mob) final;
 bool damage_logging_enabled(const MobRuntime& mob) final;
private:VerticalCombatClock& clock_;
};
}
