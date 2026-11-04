#pragma once
#include "eternity/game/mob_ai_states_selected.hpp"
namespace eternity::game {
// Live template byte at mob+0xa8 -> +0x60; missing from canonical template.
struct MobAiRandomDefinition {std::uint8_t normal_step=0;};
class MobAiRandomServices:public MobAiSelectedServices {
public:
    virtual std::int32_t random()=0; // Retained, serialized original CRT rand owner.
};
// CMob::onMoveRandom 004afcf0 / onMoveRandomRun 004aff90.
void update_mob_ai_random(MobRuntime&,const MobAiRandomDefinition&,const MapDefinition&,bool run,MobAiRandomServices&);
class MobAiRandomCompiledServices final:public MobAiCompiledServices {
public:
    MobAiRandomCompiledServices(const MobAiRandomDefinition& definition,const MapDefinition& map,MobAiRandomServices& movement,MobAiCompiledServices& others):definition_(definition),map_(map),movement_(movement),others_(others){}
    void invoke(MobAiCompiledFunction,MobRuntime&,MobAiConfiguredFsm&) override;
private:
    const MobAiRandomDefinition& definition_;
    const MapDefinition& map_;
    MobAiRandomServices& movement_;
    MobAiCompiledServices& others_;
};
}
