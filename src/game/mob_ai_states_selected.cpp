#include "eternity/game/mob_ai_states_selected.hpp"
#include "eternity/game/zone_mobs.hpp"
#include <cmath>
#include <limits>
namespace eternity::game {
namespace {
float distance(UserPosition a,UserPosition b){
    const auto x=static_cast<float>(static_cast<double>(a.x)-b.x);
    const auto z=static_cast<float>(static_cast<double>(a.z)-b.z);
    return static_cast<float>(std::sqrt(static_cast<double>(x)*x+static_cast<double>(z)*z));
}
std::int32_t integer(float value){
    const auto n=std::trunc(static_cast<double>(value));
    if(!std::isfinite(n)||n<std::numeric_limits<std::int32_t>::min()||n>std::numeric_limits<std::int32_t>::max())return std::numeric_limits<std::int32_t>::min();
    return static_cast<std::int32_t>(n);
}
}
void update_mob_ai_selected(MobRuntime& mob,const MapDefinition& map,MobAiSelectedServices& world){
    if(!mob.zone)return;
    if(!mob.in_battle){
        const auto home=distance(mob.respawn_position,mob.position);
        if((mob.trail.current().has_value()&&home>0.5F)||home>20.0F){
            if(mob_status(mob)!=2)mob.status=3;
            return;
        }
    }
    mob.selected_position.y=0;
    // Original unordered x87 comparison also takes this rejection branch.
    if(!(distance(mob.selected_position,mob.respawn_position)<20.0F)){
        mob.selected_position=mob.position;
        return;
    }
    const MapTile from{integer(mob.position.x),integer(mob.position.z)};
    const MapTile to{integer(mob.selected_position.x),integer(mob.selected_position.z)};
    if(!map.enable_line(from,to)){
        mob.selected_position=mob.position;
        return;
    }
    world.move(mob,mob.selected_position,1);
}
void MobAiSelectedCompiledServices::invoke(MobAiCompiledFunction function,MobRuntime& mob,MobAiConfiguredFsm& owner){
    if(function==MobAiCompiledFunction::move_selected)update_mob_ai_selected(mob,map_,movement_);
    else others_.invoke(function,mob,owner);
}
}
