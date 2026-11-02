#include "eternity/game/npc_targets.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace eternity::game {
namespace {
std::int32_t truncate(float value) {
    const auto integral=std::trunc(static_cast<double>(value));
    if(!std::isfinite(integral) || integral<INT32_MIN || integral>INT32_MAX) return INT32_MIN;
    return static_cast<std::int32_t>(integral);
}
float distance(UserPosition a,UserPosition b) {
    const auto x=static_cast<float>(static_cast<double>(a.x)-b.x),z=static_cast<float>(static_cast<double>(a.z)-b.z);
    return static_cast<float>(std::sqrt(static_cast<double>(x)*x+static_cast<double>(z)*z));
}
bool in_range(const Npc& npc,UserPosition position) {
    if(!(distance(position,npc.position)<=20.0F)) return false;
    if(!npc.source || npc.next_position<0 || static_cast<std::size_t>(npc.next_position)>=npc.source->positions.size()) throw std::logic_error("NPC target query has no original route point");
    return distance(position,npc.source->positions[static_cast<std::size_t>(npc.next_position)].position)<=30.0F;
}
}
void set_npc_target(Npc& npc,Actor& target,const NpcClock& clock) {
    // SetTarget(CUser*) 004bc750. This does not reapply search filters/ranges.
    if(npc.status==1 || target.attack_state().mode==1) return;
    if(npc.status!=2) npc.status=1;
    npc.target_kind=1;npc.target_id=target.id();target.guard_target_after()=clock.tick_ms()+5000U;
}
void set_npc_target(Npc& npc,NpcMobTarget& target,const NpcClock& clock) {
    // SetTarget(CMob*) 004bc790 reads the live top-of-FSM status again.
    if(npc.status==1 || target.status()==2) return;
    if(npc.status!=2) npc.status=1;
    npc.target_kind=2;npc.target_id=target.id();target.guard_target_after()=clock.tick_ms()+5000U;
}
bool find_npc_target(Npc& npc,const MapDefinition& map,ZoneUsers& users,NpcMobCells& mobs,const NpcClock& clock) {
    if(map.blocked({truncate(npc.position.x),truncate(npc.position.z)})) return false;
    if(!npc.cell || !npc.definition) throw std::logic_error("NPC target query requires bound zone/definition");
    const auto cell=*npc.cell;const auto count=map.geometry().cell_count();
    for(auto z=std::max(cell.z-1,0);z<=std::min(cell.z+1,count-1);++z) for(auto x=std::max(cell.x-1,0);x<=std::min(cell.x+1,count-1);++x) {
        if(users.visit_cell(x,z,[&](Actor& actor,UserPosition position) {
            if(npc.definition->country()==actor.appearance().identity.country || actor.attack_state().mode==1 || actor.status().shape!=0 || actor.status().no_mob_targeting || !actor.appearance().attackable || !actor.appearance().visible) return false;
            if(!in_range(npc,position)) return false;
            const auto tick=clock.tick_ms();if(actor.guard_target_after()>tick) return false;
            set_npc_target(npc,actor,clock);return true;
        })) return true;
        if(mobs.visit_cell({x,z},[&](NpcMobTarget& mob) {
            const auto country=npc.definition->country();
            if((country!=2 && country==mob.country()) || mob.status()==2 || !in_range(npc,mob.position())) return false;
            const auto tick=clock.tick_ms();if(mob.guard_target_after()>tick) return false;
            set_npc_target(npc,mob,clock);return true;
        })) return true;
    }
    return false;
}
}
