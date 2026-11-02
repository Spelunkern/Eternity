#pragma once
#include "eternity/game/mob_pool.hpp"
#include "eternity/game/zone_mobs.hpp"

namespace eternity::game {
struct MobPopulationArea {
    MapBox box;std::int32_t delta_x=1,delta_z=1;
    std::vector<MobRespawn> spawns;
};
// Per-zone copies of regular map respawns. Map/definitions, zone and pool must
// outlive their spawned objects. Do not resize area/spawn storage after spawning.
class MobPopulation {
public:
    MobPopulation(const MapDefinition&,const MobRuntimeTemplates&);
    MobPopulation(const MobPopulation&)=delete;MobPopulation& operator=(const MobPopulation&)=delete;
    std::span<MobPopulationArea> areas() noexcept {return areas_;}
    std::span<const MobPopulationArea> areas() const noexcept {return areas_;}
    // CZone::MapInitObject 0041c5f0. Required linked-group initialization follows
    // regular spawns even when every attempt fails; its bool result is ignored.
    // Caller supplies the zone for this map and a shared executor random source.
    bool initialize(ZoneMobs&,MobPool&,MobInitPorts&,const std::function<bool()>& linked_groups);
private:
    const MapDefinition& map_;std::vector<MobPopulationArea> areas_;
};
}
