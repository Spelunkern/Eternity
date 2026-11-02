#include "eternity/game/npc_navigation.hpp"
#include <bit>
#include <cmath>
#include <stdexcept>

namespace eternity::game {
namespace {
std::int32_t trunc32(double value) {
    value=std::trunc(value);if(!std::isfinite(value) || value<INT32_MIN || value>INT32_MAX) return INT32_MIN;
    return static_cast<std::int32_t>(value);
}
std::int32_t ftol_low(float value) {
    // _ftol2 0051c424 returns a QWORD; these callers use only its low DWORD.
    const auto integral=std::trunc(static_cast<double>(value));
    if(!std::isfinite(integral) || integral<-9223372036854775808.0 || integral>=9223372036854775808.0) return 0;
    return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(static_cast<std::int64_t>(integral)));
}
std::int32_t sub(std::int32_t a,std::int32_t b) {return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(a)-static_cast<std::uint32_t>(b));}
float distance(UserPosition a,UserPosition b) {
    const auto x=static_cast<float>(static_cast<double>(a.x)-b.x),z=static_cast<float>(static_cast<double>(a.z)-b.z);
    return static_cast<float>(std::sqrt(static_cast<double>(x)*x+static_cast<double>(z)*z));
}
struct Step {UserPosition delta,direction;float length=0;};
Step step_to(const Npc& npc,MapTile point) {
    Step result;result.delta={static_cast<float>(sub(point.x,trunc32(npc.position.x))),0,static_cast<float>(sub(point.z,trunc32(npc.position.z)))};
    const auto x=result.delta.x,z=result.delta.z;
    result.length=static_cast<float>(std::sqrt(static_cast<double>(static_cast<float>(static_cast<double>(x)*x+static_cast<double>(z)*z))));
    if(x!=0 || z!=0) result.direction={static_cast<float>(static_cast<double>(x)/result.length),static_cast<float>(0.0/static_cast<double>(result.length)),static_cast<float>(static_cast<double>(z)/result.length)};
    return result;
}
UserPosition plus(UserPosition a,UserPosition b) {return {static_cast<float>(static_cast<double>(a.x)+b.x),static_cast<float>(static_cast<double>(a.y)+b.y),static_cast<float>(static_cast<double>(a.z)+b.z)};}
UserPosition minus(UserPosition a,UserPosition b) {return {static_cast<float>(static_cast<double>(a.x)-b.x),static_cast<float>(static_cast<double>(a.y)-b.y),static_cast<float>(static_cast<double>(a.z)-b.z)};}
UserPosition multiply(UserPosition a,float value) {return {static_cast<float>(static_cast<double>(a.x)*value),static_cast<float>(static_cast<double>(a.y)*value),static_cast<float>(static_cast<double>(a.z)*value)};}
void require_definition(const Npc& npc) {if(!npc.definition) throw std::logic_error("NPC navigation has no original definition");}
MapTile current_tile(const Npc& npc) {return {trunc32(npc.position.x),trunc32(npc.position.z)};}
MapTile waypoint(const Npc& npc) {const auto point=npc.path.current();if(!point) throw std::logic_error("NPC navigation has a found path without a cursor");return *point;}
void status(Npc& npc,std::int32_t value) {if(npc.status!=2) npc.status=value;}
void stop(Npc& npc) {if(npc.status!=2) {npc.status=0;npc.target_kind=0;npc.target_id=0;}}
std::uint32_t scaled_deadline(float distance,float range,std::int32_t interval,std::uint32_t tick) {
    const auto quarter=static_cast<float>(interval/4);const auto base=static_cast<float>(tick);
    return static_cast<std::uint32_t>(trunc32((static_cast<double>(distance)/range)*quarter+base));
}
}
bool chase_npc(Npc& npc,UserPosition target,std::int32_t range,std::uint32_t tick,const MapDefinition& map,NpcNavigationWorld& world) {
    if(tick<npc.next_chase) return false;
    if(map.blocked(current_tile(npc))) {stop(npc);return false;}
    const auto separation=distance(target,npc.position);
    if(static_cast<double>(static_cast<float>(range))+1.0>separation) {npc.path.reset();return true;}
    if(!npc.source || npc.next_position<0 || static_cast<std::size_t>(npc.next_position)>=npc.source->positions.size()) throw std::logic_error("NPC chase has no original route point");
    if(distance(target,npc.source->positions[static_cast<std::size_t>(npc.next_position)].position)>30 || separation>30) {status(npc,3);npc.target_kind=0;npc.path.reset();return false;}
    MapTile point{ftol_low(target.x),ftol_low(target.z)};
    if(!npc.path.found()) {
        const MapTile from{ftol_low(npc.position.x),ftol_low(npc.position.z)};MapTile previous;
        if(!map.enable_line(from,point,&previous)) {
            if(map.blocked(point)) {
                if(previous.x==from.x && previous.z==from.z) {status(npc,3);npc.target_kind=0;}else stop(npc);
                npc.path.reset();return false;
            }
            if(!npc.path.find(map,from,point)) {status(npc,3);npc.target_kind=0;npc.path.reset();npc.disable_time=world.tick_ms()+3000U;return false;}
            point=waypoint(npc);
        }
    } else point=waypoint(npc);
    require_definition(npc);const auto step=step_to(npc,point);const auto remaining=static_cast<float>(static_cast<double>(step.length)-1.0);
    npc.trail.add(map,current_tile(npc));const auto limit=static_cast<float>(npc.definition->move_distance());
    if(remaining<limit) {
        const auto old=npc.position;const auto destination=minus(plus(step.delta,old),multiply(step.direction,0.3F));
        if(world.move(npc,destination,1)) npc.position=minus(plus(step.delta,old),multiply(step.direction,0.0F));
        npc.next_chase=scaled_deadline(remaining,limit,npc.definition->move_speed(),tick);npc.path.advance();
    } else {
        world.move(npc,plus(multiply(step.direction,limit),npc.position),1);npc.next_chase=tick+static_cast<std::uint32_t>(npc.definition->move_speed()/4);
    }
    return false;
}
bool return_npc_home(Npc& npc,UserPosition home,std::uint32_t tick,const MapDefinition& map,NpcNavigationWorld& world) {
    if(tick<npc.next_chase) return false;
    if(distance(home,npc.position)<1.5F) {status(npc,0);npc.path.reset();npc.trail.clear();return true;}
    MapTile point{ftol_low(home.x),ftol_low(home.z)};
    if(!npc.path.found()) {
        const MapTile from{ftol_low(npc.position.x),ftol_low(npc.position.z)};MapTile previous;
        if(!map.enable_line(from,point,&previous)) {
            if(map.blocked(point)) {status(npc,0);npc.path.reset();npc.trail.clear();return false;}
            if(const auto trail=npc.trail.pop()) {point=*trail;npc.path.set_path(point);}
            else {
                if(!npc.path.find(map,from,point)) {status(npc,0);npc.path.reset();npc.trail.clear();npc.disable_time=world.tick_ms()+3000U;return false;}
                point=waypoint(npc);
            }
        }
    } else point=waypoint(npc);
    require_definition(npc);const auto step=step_to(npc,point);const auto limit=static_cast<float>(npc.definition->move_distance());
    if(step.length<limit) {
        world.move(npc,plus(step.delta,npc.position),1);npc.next_chase=scaled_deadline(step.length,limit,npc.definition->move_speed(),tick);npc.path.advance();
    } else {
        world.move(npc,plus(multiply(step.direction,limit),npc.position),1);npc.next_chase=tick+static_cast<std::uint32_t>(npc.definition->move_speed()/4);
    }
    return false;
}
}
