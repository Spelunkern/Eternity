#include "eternity/game/mob_ai_states_move.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <stdexcept>
namespace eternity::game {
namespace {
std::int32_t trunc32(double v) {v=std::trunc(v);return !std::isfinite(v)||v<INT32_MIN||v>INT32_MAX?INT32_MIN:static_cast<std::int32_t>(v);}
std::int32_t ftol(float v) {const auto n=std::trunc(static_cast<double>(v));return !std::isfinite(n)||n<-9223372036854775808.0||n>=9223372036854775808.0?0:std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(static_cast<std::int64_t>(n)));}
std::int32_t sub(std::int32_t a,std::int32_t b){return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(a)-static_cast<std::uint32_t>(b));}
float distance(UserPosition a,UserPosition b){const auto x=static_cast<float>(static_cast<double>(a.x)-b.x),z=static_cast<float>(static_cast<double>(a.z)-b.z);return static_cast<float>(std::sqrt(static_cast<double>(x)*x+static_cast<double>(z)*z));}
MapTile waypoint(const MobRuntime& m){const auto p=m.path.current();if(!p)throw std::logic_error("Found mob movement path has no cursor");return *p;}
void damage_reset(MobRuntime& m){if(!m.damage)throw std::logic_error("Mob movement requires its damage owner");m.damage->reset();}
}
void update_mob_ai_move(MobRuntime& m,const MobAiMoveDefinition& definition,const MapDefinition& map,MobAiMoveServices& world){
    const auto now=world.tick();
    const bool reset=m.move_mode==3;
    if((m.move_mode==2||m.move_mode==3) && world.find_target(m)){world.dispatch(m,0,m.target_id);return;}
    if(now<m.next_chase)return;
    const auto target=m.move_target;
    const auto separation=distance(target,m.position);
    const auto finish=[&]{
        world.dispatch(m,6,0);m.respawn_position=m.position;m.path.reset();m.trail.clear();
        if(reset){world.reset_attack(m);damage_reset(m);}
        else dispatch_mob_event(m,MobEvent::move_end_lua,0,world);
    };
    if((m.trail.current().has_value() && separation<0.5F) || separation<static_cast<float>(world.random()%5+5)){finish();return;}
    if(m.stop!=0||m.sleep!=0||m.stun!=0)return;
    MapTile point;
    if(!m.path.found()){
        point={trunc32(target.x),trunc32(target.z)};
        const MapTile from{ftol(m.position.x),ftol(m.position.z)};
        if(!map.enable_line(from,point)){
            if(auto trail=m.trail.pop()){point=*trail;m.path.set_path(point);}
            else {
                if(!m.path.find(map,from,point)){finish();return;}
                point=waypoint(m);
            }
        }
    }else point=waypoint(m);
    const UserPosition delta{static_cast<float>(sub(point.x,trunc32(m.position.x))),0,static_cast<float>(sub(point.z,trunc32(m.position.z)))};
    const auto sum=static_cast<float>(static_cast<double>(delta.x)*delta.x+static_cast<double>(delta.z)*delta.z);
    const auto norm=static_cast<float>(std::sqrt(static_cast<double>(sum)));
    UserPosition direction;
    if(delta.x!=0||delta.z!=0)direction={static_cast<float>(static_cast<double>(delta.x)/norm),static_cast<float>(0.0/static_cast<double>(norm)),static_cast<float>(static_cast<double>(delta.z)/norm)};
    constexpr std::array<std::int32_t,5> speeds{-4,-2,0,2,4};
    const auto limit=static_cast<float>(definition.normal_step+speeds[static_cast<std::size_t>(std::clamp(m.move_speed,0,4))]);
    const auto remaining=static_cast<float>(static_cast<double>(norm)-(static_cast<double>(static_cast<float>(definition.size))+1.0));
    if(remaining<limit){
        const UserPosition destination{static_cast<float>(point.x),m.position.y,static_cast<float>(point.z)};
        if(world.move(m,destination,0)){m.position.x=destination.x;m.position.z=destination.z;}
        m.next_chase=static_cast<std::uint32_t>(trunc32((static_cast<double>(remaining)/limit)*static_cast<float>(definition.normal_time)+static_cast<float>(now)));
        m.path.advance();
    }else {
        const UserPosition step{static_cast<float>(static_cast<double>(direction.x)*limit),static_cast<float>(static_cast<double>(direction.y)*limit),static_cast<float>(static_cast<double>(direction.z)*limit)};
        const UserPosition destination{static_cast<float>(static_cast<double>(step.x)+m.position.x),static_cast<float>(static_cast<double>(step.y)+m.position.y),static_cast<float>(static_cast<double>(step.z)+m.position.z)};
        world.move(m,destination,0);m.next_chase=now+definition.normal_time;
    }
}
}
