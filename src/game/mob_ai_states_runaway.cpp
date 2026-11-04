#include "eternity/game/mob_ai_states_runaway.hpp"
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
void update_mob_ai_run_away(MobRuntime& mob,const MapDefinition& map,MobAiSelectedServices& world){
    if(!mob.zone)return;
    const UserPosition destination{
        static_cast<float>(static_cast<double>(mob.position.x)+mob.selected_position.x),
        static_cast<float>(static_cast<double>(mob.position.y)+mob.selected_position.y),
        static_cast<float>(static_cast<double>(mob.position.z)+mob.selected_position.z)};
    if(!mob.in_battle){
        const auto home=distance(mob.respawn_position,mob.position);
        if((mob.trail.current().has_value()&&home>0.5F)||home>20.0F){
            if(mob_status(mob)!=2)mob.status=3;
            return;
        }
    }
    if(!(distance(destination,mob.respawn_position)<20.0F))return;
    if(map.enable_line({integer(mob.position.x),integer(mob.position.z)},
                       {integer(destination.x),integer(destination.z)}))world.move(mob,destination,1);
}
void MobAiRunAwayCompiledServices::invoke(MobAiCompiledFunction function,MobRuntime& mob,MobAiConfiguredFsm& owner){
    if(function==MobAiCompiledFunction::move_run_away)update_mob_ai_run_away(mob,map_,movement_);
    else others_.invoke(function,mob,owner);
}
}
