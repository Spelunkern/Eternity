#pragma once
#include "eternity/game/mob_ai_states_bindings.hpp"
namespace eternity::game {
// CKFSM<CMob>::AddState 004adef0: hash the state's C name, then insert a
// retained value copy. A duplicate key never replaces the admitted value.
// Condition metadata/tokens must already be decoded as owned native values.
// External function/variable bindings retain their service lifetime contracts;
// this API does not reinterpret original vector pointers as native addresses.
void admit_mob_ai_state(MobAiConfiguredFsm&,const MobAiConfiguredState&);
}
