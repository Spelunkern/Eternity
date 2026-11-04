#include "eternity/game/mob_ai_states_return.hpp"
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
MapTile waypoint(const MobRuntime& m){const auto p=m.path.current();if(!p)throw std::logic_error("Found mob return path has no cursor");return *p;}
void damage_reset(MobRuntime& m){if(!m.damage)throw std::logic_error("Mob return requires its damage owner");m.damage->reset();}
}
void update_mob_ai_return_home(MobRuntime& m,const MobAiReturnDefinition& definition,const MapDefinition& map,MobAiReturnServices& world){
    const auto now=world.tick();if(now<m.next_chase)return;
    const auto separation=distance(m.respawn_position,m.position);
    const bool had_trail=m.trail.current().has_value();
    if((had_trail && separation<0.5F)||separation<10.0F){
        world.reset_attack(m);m.path.reset();m.trail.clear();damage_reset(m);world.dispatch(m,2,0);return;
    }
    if(m.stop!=0||m.sleep!=0||m.stun!=0)return;
    MapTile point;
    if(!m.path.found()){
        point={ftol(m.respawn_position.x),ftol(m.respawn_position.z)};
        const MapTile from{ftol(m.position.x),ftol(m.position.z)};
        // EnableLine leaves output scratch untouched outside the map; the
        // original then compares that uninitialized stack storage. Do not invent
        // a deterministic replacement for that unrecovered invalid-world path.
        if(from.x<0||from.z<0||from.x>=map.size||from.z>=map.size||point.x<0||point.z<0||point.x>=map.size||point.z>=map.size)throw std::logic_error("Mob return line endpoints are outside the recovered map contract");
        MapTile previous{};
        if(!map.enable_line(from,point,&previous)){
            if(map.blocked(point)){
                if(previous.x==from.x && previous.z==from.z){m.respawn_position=m.position;m.path.reset();m.trail.clear();}
                else {m.trail.clear();m.path.reset();}
                world.dispatch(m,2,0);return;
            }
            if(had_trail){const auto path=m.trail.pop();if(!path)throw std::logic_error("Mob return lost its retained trail");point=*path;m.path.set_path(point);}
            else {
                if(!m.path.find(map,from,point)){m.respawn_position=m.position;m.path.reset();m.trail.clear();world.dispatch(m,2,0);return;}
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
    const auto limit=static_cast<float>(definition.chase_step+speeds[static_cast<std::size_t>(std::clamp(m.move_speed,0,4))]);
    const auto length=norm;
    const auto remaining=static_cast<float>(static_cast<double>(length)-(static_cast<double>(static_cast<float>(definition.size))+1.0));
    if(remaining<limit){
        const UserPosition destination{static_cast<float>(point.x),m.position.y,static_cast<float>(point.z)};
        if(world.move(m,destination,1)){m.position.x=destination.x;m.position.z=destination.z;}
        m.next_chase=static_cast<std::uint32_t>(trunc32((static_cast<double>(remaining)/limit)*static_cast<float>(definition.chase_time)+static_cast<float>(now)));
        m.path.advance();
    }else {
        const UserPosition step{static_cast<float>(static_cast<double>(direction.x)*limit),static_cast<float>(static_cast<double>(direction.y)*limit),static_cast<float>(static_cast<double>(direction.z)*limit)};
        const UserPosition destination{static_cast<float>(static_cast<double>(step.x)+m.position.x),static_cast<float>(static_cast<double>(step.y)+m.position.y),static_cast<float>(static_cast<double>(step.z)+m.position.z)};
        world.move(m,destination,1);m.next_chase=now+definition.chase_time;
    }
}
}
