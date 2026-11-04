#pragma once
#include "eternity/game/mob_ai_states_selected.hpp"
namespace eternity::game {
// CMob::onMoveRunAway 004b0360: selected_position is a displacement here.
void update_mob_ai_run_away(MobRuntime&,const MapDefinition&,MobAiSelectedServices&);
class MobAiRunAwayCompiledServices final:public MobAiCompiledServices {
public:
    MobAiRunAwayCompiledServices(const MapDefinition& map,MobAiSelectedServices& movement,MobAiCompiledServices& others):map_(map),movement_(movement),others_(others){}
    void invoke(MobAiCompiledFunction,MobRuntime&,MobAiConfiguredFsm&) override;
private:
    const MapDefinition& map_;
    MobAiSelectedServices& movement_;
    MobAiCompiledServices& others_;
};
}
