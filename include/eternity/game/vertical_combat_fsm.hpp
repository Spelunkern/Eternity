#pragma once
#include "eternity/game/vertical_combat_actor.hpp"
#include "eternity/game/mob_ai_states.hpp"
namespace eternity::game {
// CMob::Damage 00490ee0 dispatches event 3 at 00490f4b when the
// mob's stack FSM is active. Reuse the existing live stack transition and its
// mandatory Lua/tick/error services; retain mob and all owners across reentry.
class VerticalCombatFsmHitServices:public VerticalCombatActorHitServices {
public:
 VerticalCombatFsmHitServices(Actor& actor,PartyMembershipUser& member,const MobDeathExperience& rules,MobDeathWorld& world,MobAiStateServices& fsm)
  :VerticalCombatActorHitServices(actor,member,rules,world),fsm_(fsm){}
 void dispatch_death_event(MobRuntime& mob) final {dispatch_mob_ai_event(mob,3,0,fsm_);}
private:MobAiStateServices& fsm_;
};
}
