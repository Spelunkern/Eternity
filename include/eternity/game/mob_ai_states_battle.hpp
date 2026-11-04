#pragma once
#include "eternity/game/mob_ai_states_bindings.hpp"
#include "eternity/game/mob_ai_states_reset.hpp"
namespace eternity::game {
// Live views of canonical template/user fields not yet exposed by MobRuntime.
// Template country: mob+0xa8 -> +0x78. User: country +0x12d, life +0x142c,
// immunity +0x1366, admin attackable +0x5775 and visible +0x5774.
struct MobAiBattleDefinition {std::uint32_t country=0;};
struct MobAiBattleTarget {
    std::uint32_t id=0;
    std::int32_t life_status=0;
    std::uint8_t country=0,immunity=0,attackable=0,visible=0;
};
class MobAiBattleServices:public MobAiConfiguredServices {
public:
    virtual MobAiBattleTarget* find_user(MobRuntime&,std::uint32_t)=0;
    virtual void remove_vision_fix(MobRuntime&)=0;
    virtual MobAiBattleTarget* find_target(MobRuntime&)=0;
    virtual void attack_ai(MobRuntime&,std::uint32_t,MobAiBattleTarget&)=0;
};
// Original compiled callbacks: onBattle 004b0520 and PAI_Attack 004af9c0.
// Views, targets, catalog, subject and services must survive nested callbacks.
void attack_mob_ai_battle(MobRuntime&,MobAiConfiguredFsm&,const MobAiBattleDefinition&,
    const MobAiVariableCatalog&,std::uint32_t,MobAiBattleTarget&,MobAiBattleServices&);
void update_mob_ai_battle(MobRuntime&,MobAiConfiguredFsm&,const MobAiBattleDefinition&,
    const MobAiVariableCatalog&,MobAiBattleServices&);
// Compose the real Battle body with the existing retained compiled registry.
// Other compiled bodies remain required; this adapter supplies no fallback.
class MobAiBattleCompiledServices final:public MobAiCompiledServices {
public:
    MobAiBattleCompiledServices(const MobAiBattleDefinition& definition,const MobAiVariableCatalog& catalog,
        MobAiBattleServices& battle,MobAiCompiledServices& others):definition_(definition),catalog_(catalog),battle_(battle),others_(others){}
    void invoke(MobAiCompiledFunction,MobRuntime&,MobAiConfiguredFsm&) override;
private:
    const MobAiBattleDefinition& definition_;
    const MobAiVariableCatalog& catalog_;
    MobAiBattleServices& battle_;
    MobAiCompiledServices& others_;
};
}
