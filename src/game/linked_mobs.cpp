#include "eternity/game/linked_mobs.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace eternity::game {
namespace {
std::size_t member_count(const LinkedMobSpawn& spawn) {if(spawn.count<0 || spawn.count>16) throw std::invalid_argument("Invalid linked mob count");return static_cast<std::size_t>(spawn.count);}
std::int32_t tile(float v) {const auto n=std::trunc(static_cast<double>(v));return !std::isfinite(n) || n<INT32_MIN || n>INT32_MAX?INT32_MIN:static_cast<std::int32_t>(n);}
float position(RandomSource& random,const LinkedMobSpawn& spawn,float center) {
    // x87 spills the random remainder and final position, retaining double
    // precision across radius subtraction and center addition.
    const auto offset=static_cast<float>(random.next()%spawn.radius_twice);
    return static_cast<float>((static_cast<double>(offset)-spawn.radius)+center);
}
}
void LinkedMobSpawn::reset_members() {std::fill_n(members.begin(),member_count(*this),nullptr);}
void LinkedMobSpawn::add(MobRuntime* mob) {const auto end=members.begin()+static_cast<std::ptrdiff_t>(member_count(*this));const auto at=std::find(members.begin(),end,nullptr);if(at!=end) *at=mob;}
void LinkedMobSpawn::remove(MobRuntime* mob) {const auto end=members.begin()+static_cast<std::ptrdiff_t>(member_count(*this));const auto at=std::find(members.begin(),end,mob);if(at!=end) *at=nullptr;}
LinkedMobPopulation::LinkedMobPopulation(std::span<const LinkedMobGroup> groups):groups_(groups.begin(),groups.end()) {
    // CZone::MapInit 0041c8a0 repairs self pointers after the stMapBoth copy.
    for(auto& group:groups_) {
        if(group.spawn_count<0 || group.spawn_count>32) throw std::invalid_argument("Invalid linked mob group count");
        member_count(group.leader);group.leader.respawn.both_source=&group.leader;
        for(std::int32_t i=0;i<group.spawn_count;++i) {auto& spawn=group.spawns[static_cast<std::size_t>(i)];member_count(spawn);spawn.respawn.both_source=&spawn;}
    }
}
bool LinkedMobPopulation::initialize(const MapDefinition& map,ZoneMobs& zone,MobPool& pool,MobInitPorts& ports,const std::function<std::uint32_t()>& unix_time) {
    if(!unix_time) throw std::invalid_argument("Missing linked mob clock");
    ServerRandom timer_random(unix_time()); // Constructor samples time even with no groups.
    for(auto& group:groups_) {
        group.current_index=0;auto& leader=group.leader;const auto* definition=leader.definitions[0];if(!definition) continue;
        if(leader.randomized!=0) {
            auto delay=leader.minimum_minutes*60000U;
            if(leader.random_minutes!=0) {const auto bound=leader.random_minutes*60U;if(bound==0) throw std::invalid_argument("Invalid linked mob random timer");timer_random.seed(unix_time()^definition->id);delay+=timer_random.next(bound)%bound*1000U;}
            leader.respawn.next=ports.tick()+delay;leader.respawn.current=0;continue;
        }
        leader.respawn.next=0;leader.respawn.current=1;auto* mob=pool.allocate();if(!mob) continue;
        mob->definition=definition;initialize_mob(*mob,definition->id,&leader.respawn,ports);mob->position=leader.center;leader.members[0]=mob;static_cast<void>(zone.enter(*mob));
        for(std::int32_t i=0;i<group.spawn_count;++i) {
            auto& spawn=group.spawns[static_cast<std::size_t>(i)];definition=spawn.definitions[0];if(!definition) continue;
            spawn.respawn.next=0;spawn.respawn.current=spawn.count;spawn.reset_members();
            if(spawn.count>0 && spawn.radius_twice==0) throw std::invalid_argument("Invalid linked mob radius");
            for(std::int32_t n=0;n<spawn.count;++n) {
                const auto x=position(ports.random,spawn,spawn.center.x),z=position(ports.random,spawn,spawn.center.z);const MapTile at{tile(x),tile(z)};
                if(at.x<0 || at.z<0 || at.x>=map.size || at.z>=map.size || map.blocked(at)) continue;
                mob=pool.allocate();if(!mob) continue;
                mob->definition=definition;initialize_mob(*mob,definition->id,&spawn.respawn,ports);mob->position={x,spawn.center.y,z};spawn.add(mob);static_cast<void>(zone.enter(*mob));
            }
        }
    }
    return true;
}
}
