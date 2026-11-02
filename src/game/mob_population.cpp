#include "eternity/game/mob_population.hpp"
#include <cmath>
#include <stdexcept>

namespace eternity::game {
namespace {
std::int32_t tile(float value) {
    const auto integral=std::trunc(static_cast<double>(value));
    if(!std::isfinite(integral) || integral<INT32_MIN || integral>INT32_MAX) return INT32_MIN;
    return static_cast<std::int32_t>(integral);
}
float placement(RandomSource& random,std::int32_t delta,float minimum) {
    const auto offset=static_cast<float>(random.next()%delta);
    return static_cast<float>(static_cast<double>(offset)+minimum);
}
}
MobPopulation::MobPopulation(const MapDefinition& map,const MobRuntimeTemplates& definitions):map_(map) {
    for(const auto& source:map.mobs) {
        if(source.spawns.empty()) continue; // MapInit/stMapMob::Copy skips empty areas.
        if(source.delta_x<1 || source.delta_z<1) throw std::invalid_argument("Invalid decoded mob area extent");
        MobPopulationArea area;area.box=source.box;area.delta_x=source.delta_x;area.delta_z=source.delta_z;
        for(const auto& spawn:source.spawns) {MobRespawn respawn;respawn.type=spawn.mob_id;respawn.count=spawn.count;respawn.definition=definitions.find(static_cast<std::uint32_t>(spawn.mob_id));area.spawns.push_back(respawn);}
        areas_.push_back(std::move(area));
    }
}
bool MobPopulation::initialize(ZoneMobs& zone,MobPool& pool,MobInitPorts& ports,const std::function<bool()>& linked_groups) {
    if(!linked_groups) throw std::invalid_argument("Missing linked mob initialization");
    for(auto& area:areas_) for(auto& spawn:area.spawns) {
        if(!spawn.definition) continue;
        spawn.next=0;spawn.current=spawn.count;
        for(std::int32_t i=0;i<spawn.count;++i) {
            const auto x=placement(ports.random,area.delta_x,area.box.minimum.x);
            const auto z=placement(ports.random,area.delta_z,area.box.minimum.z);
            const MapTile position{tile(x),tile(z)};
            if(position.x<0 || position.z<0 || position.x>=map_.size || position.z>=map_.size || map_.blocked(position)) continue;
            auto* mob=pool.allocate();if(!mob) continue;
            mob->definition=spawn.definition;initialize_mob(*mob,spawn.definition->id,&spawn,ports);
            mob->position={x,area.box.minimum.y,z};static_cast<void>(zone.enter(*mob));
        }
    }
    static_cast<void>(linked_groups());return true;
}
}
