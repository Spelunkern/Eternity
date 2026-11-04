#include "eternity/game/mob_ai_states_battle.hpp"
namespace eternity::game {
void attack_mob_ai_battle(MobRuntime& mob,MobAiConfiguredFsm& fsm,const MobAiBattleDefinition& definition,
    const MobAiVariableCatalog& catalog,std::uint32_t now,MobAiBattleTarget& target,MobAiBattleServices& services){
    if(definition.country!=target.country && target.life_status!=1 && target.immunity==0 && target.attackable!=0 && target.visible!=0){
        services.attack_ai(mob,now,target);
        return;
    }
    if(!services.find_target(mob))reset_mob_ai_configured(mob,fsm,catalog,services);
}
void update_mob_ai_battle(MobRuntime& mob,MobAiConfiguredFsm& fsm,const MobAiBattleDefinition& definition,
    const MobAiVariableCatalog& catalog,MobAiBattleServices& services){
    if(!mob.zone || mob.next_attack>mob.current_time || mob.sleep!=0 || mob.stun!=0)return;
    // 004b054a..004b055f: 0xff is compared only inside the 0..6 branch.
    // As in WhileCombat, this cannot reject any signed attack-speed value.
    auto* target=services.find_user(mob,mob.target_id);
    if(!target || target->life_status==1){
        services.remove_vision_fix(mob);
        if(!services.find_target(mob))reset_mob_ai_configured(mob,fsm,catalog,services);
        return;
    }
    // Reload current_time after lookup, exactly as the original callback does.
    attack_mob_ai_battle(mob,fsm,definition,catalog,mob.current_time,*target,services);
}
void MobAiBattleCompiledServices::invoke(MobAiCompiledFunction function,MobRuntime& mob,MobAiConfiguredFsm& fsm){
    if(function==MobAiCompiledFunction::battle)update_mob_ai_battle(mob,fsm,definition_,catalog_,battle_);
    else others_.invoke(function,mob,fsm);
}
}
