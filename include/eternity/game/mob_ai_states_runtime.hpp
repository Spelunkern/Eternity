#pragma once
#include "eternity/game/mob_ai_states_random.hpp"
#include "eternity/game/mob_ai_states_runaway.hpp"
#include "eternity/game/mob_ai_states_battle.hpp"
namespace eternity::game {
class MobAiDebugServices {
public:
    virtual ~MobAiDebugServices()=default;
    virtual void write(std::string_view)=0;
};
// CMob::onDebug 004afce0, original printf format literal at 00538d2c.
void debug_mob_ai(MobAiDebugServices&);
// All six enum callbacks have recovered bodies; ResetUser is already directly
// bound by ConnectToFSM. Required providers are retained references, no fallback.
class MobAiCompiledRuntime final:public MobAiCompiledServices {
public:
    MobAiCompiledRuntime(const MapDefinition& map,const MobAiRandomDefinition& random_definition,
        const MobAiBattleDefinition& battle_definition,const MobAiVariableCatalog& catalog,
        MobAiRandomServices& movement,MobAiBattleServices& battle,MobAiDebugServices& debug):
        map_(map),random_definition_(random_definition),battle_definition_(battle_definition),catalog_(catalog),movement_(movement),battle_(battle),debug_(debug){}
    void invoke(MobAiCompiledFunction,MobRuntime&,MobAiConfiguredFsm&) override;
private:
    const MapDefinition& map_;
    const MobAiRandomDefinition& random_definition_;
    const MobAiBattleDefinition& battle_definition_;
    const MobAiVariableCatalog& catalog_;
    MobAiRandomServices& movement_;
    MobAiBattleServices& battle_;
    MobAiDebugServices& debug_;
};
// Only SetFSM's final publication phase 004a9734..004a97ab. Caller must have
// already admitted locals/functions/states/programs. Does not clear or reset them.
// Subject/services/definition views must outlive all retained bound callbacks.
bool start_mob_ai_factory(MobRuntime&,MobAiConfiguredFsm&,std::string_view start_name,
    std::span<const MobAiCompiledStateNames>,MobAiCompiledServices&,MobAiConfiguredServices&);
}
