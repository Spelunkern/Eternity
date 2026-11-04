#include "eternity/game/mob_ai_states_random.hpp"
#include "eternity/game/zone_mobs.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <limits>
namespace eternity::game {
namespace {
float distance(UserPosition a,UserPosition b){const auto x=static_cast<float>(static_cast<double>(a.x)-b.x),z=static_cast<float>(static_cast<double>(a.z)-b.z);return static_cast<float>(std::sqrt(static_cast<double>(x)*x+static_cast<double>(z)*z));}
std::int32_t integer(float value){const auto n=std::trunc(static_cast<double>(value));return !std::isfinite(n)||n<std::numeric_limits<std::int32_t>::min()||n>std::numeric_limits<std::int32_t>::max()?std::numeric_limits<std::int32_t>::min():static_cast<std::int32_t>(n);}
}
void update_mob_ai_random(MobRuntime& mob,const MobAiRandomDefinition& definition,const MapDefinition& map,bool run,MobAiRandomServices& world){
    if(run&&!mob.zone)return;
    const auto angle=world.random()%10000;
    const auto extra=run?0:(world.random()%500)/100;
    constexpr std::array<std::int32_t,5> speeds{-4,-2,0,2,4};
    const auto step=static_cast<float>(speeds[static_cast<std::size_t>(std::clamp(mob.move_speed,0,4))]+definition.normal_step);
    const float length=step+static_cast<float>(extra);
    const float radians=static_cast<float>(static_cast<double>(static_cast<float>(angle))*std::bit_cast<float>(0x3c8efa35U));
    const float sine=static_cast<float>(std::sin(static_cast<double>(radians)));
    const float cosine=static_cast<float>(std::cos(static_cast<double>(radians)));
    const float x=static_cast<float>(static_cast<double>(cosine)*0.0-static_cast<double>(sine)*length);
    const float z=static_cast<float>(static_cast<double>(sine)*0.0+static_cast<double>(cosine)*length);
    const UserPosition destination{x+mob.position.x,0.0F+mob.position.y,z+mob.position.z};
    const float limit=run?20.0F:10.0F;
    if(!mob.in_battle){
        const auto home=distance(mob.respawn_position,mob.position);
        if((mob.trail.current().has_value()&&home>0.5F)||home>limit){if(mob_status(mob)!=2)mob.status=3;return;}
    }
    if(mob.zone&&distance(destination,mob.respawn_position)<limit&&
        map.enable_line({integer(mob.position.x),integer(mob.position.z)},{integer(destination.x),integer(destination.z)}))world.move(mob,destination,run?1:0);
}
void MobAiRandomCompiledServices::invoke(MobAiCompiledFunction function,MobRuntime& mob,MobAiConfiguredFsm& owner){
    if(function==MobAiCompiledFunction::move_random||function==MobAiCompiledFunction::move_random_run)update_mob_ai_random(mob,definition_,map_,function==MobAiCompiledFunction::move_random_run,movement_);
    else others_.invoke(function,mob,owner);
}
}
