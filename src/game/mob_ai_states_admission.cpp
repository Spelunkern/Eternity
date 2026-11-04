#include "eternity/game/mob_ai_states_admission.hpp"
namespace eternity::game {
void admit_mob_ai_state(MobAiConfiguredFsm& owner,const MobAiConfiguredState& state){
    owner.states.emplace(mob_ai_name_code(state.name),state);
}
}
