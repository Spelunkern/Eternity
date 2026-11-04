#pragma once
#include "eternity/game/mob_ai_states_configured.hpp"
namespace eternity::game {
// CMob::ResetFSM 00492310. AI index is captured from the live immutable
// definition before variable reset; state entry uses the existing native owner.
void reset_mob_ai_configured(MobRuntime&,MobAiConfiguredFsm&,const MobAiVariableCatalog&,MobAiConfiguredServices&);
// CMob::onResetUser 004b05b0: vision fixation prevents this target/flag reset.
void reset_mob_ai_user(MobRuntime&) noexcept;
}
