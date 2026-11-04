#include "eternity/game/mob_ai_states_reset.hpp"
#include <stdexcept>
namespace eternity::game {
void reset_mob_ai_configured(MobRuntime& mob,MobAiConfiguredFsm& fsm,const MobAiVariableCatalog& catalog,MobAiConfiguredServices& services){
    mob.attack_enabled.fill(true);mob.in_battle=false;mob.attacked=false;
    mob.target_kind=0;mob.target_id=0;mob.status=100;
    if(!mob.definition)throw std::logic_error("Configured mob reset requires the original AI definition");
    const auto ai=mob.definition->ai;
    clear_mob_ai_variables(fsm.variables,catalog,ai);
    start_mob_ai_configured(mob,fsm,services);
}
void reset_mob_ai_user(MobRuntime& mob) noexcept{
    if(mob.vision_fixed_user==0){mob.attacked=false;mob.in_battle=false;mob.target_kind=0;mob.target_id=0;}
}
}
