#pragma once
#include "eternity/game/mob_ai_states_bindings.hpp"
namespace eternity::game {
class MobAiSelectedServices {
public:
    virtual ~MobAiSelectedServices()=default;
    virtual void move(MobRuntime&,UserPosition,std::int32_t)=0;
};
// CMob::onMoveAtSelectedPosition 004b01c0. Uses retained map/trail/status owners.
void update_mob_ai_selected(MobRuntime&,const MapDefinition&,MobAiSelectedServices&);
class MobAiSelectedCompiledServices final:public MobAiCompiledServices {
public:
    MobAiSelectedCompiledServices(const MapDefinition& map,MobAiSelectedServices& movement,MobAiCompiledServices& others):map_(map),movement_(movement),others_(others){}
    void invoke(MobAiCompiledFunction,MobRuntime&,MobAiConfiguredFsm&) override;
private:
    const MapDefinition& map_;
    MobAiSelectedServices& movement_;
    MobAiCompiledServices& others_;
};
}
