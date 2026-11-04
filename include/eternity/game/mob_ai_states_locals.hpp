#pragma once
#include "eternity/game/mob_ai_states_parameters.hpp"
namespace eternity::game {
// CKFSM<CMob>::AddLocalVariable 004ade70: insertion never replaces a key.
// New copies normalize undefined inactive text/string tails; strings must
// terminate within the retained 128 bytes. Existing entries remain untouched.
bool add_mob_ai_local(MobAiVariables&,std::uint32_t,const MobAiParameter&);
// Only the local-copy phase of CAIFactory::SetFSM 004a9130 (through 004a91e0).
// Does not install functions/states or publish the FSM subject/start code.
bool install_mob_ai_locals(MobAiVariables&,const MobAiVariableCatalog&,std::int32_t);
}
