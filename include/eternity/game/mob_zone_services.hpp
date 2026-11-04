#pragma once
#include "eternity/game/zones.hpp"
namespace eternity::game {
struct MobZoneServicesPorts {
    std::shared_ptr<ZoneMobServices> mobs;
    std::shared_ptr<ZoneNpcServices> npcs;
    std::shared_ptr<ZoneCorpseServices> corpses;
    std::shared_ptr<ZoneItemServices> items;
};
// Every reference borrows from lifetime. It must retain the real templates,
// catalog, AI/Init ports, clocks, heaps and all world providers through pool death.
// Clock/RNG providers must not throw (matching original primitive services).
// AI callbacks may throw before entry; the factory recycles that subject.
// The SAME bound pool is transferred to MobPrimaryZones after constructing this.
struct MobZoneServicesInputs {
    std::shared_ptr<void> lifetime;
    MobPool& pool;
    const MobRuntimeTemplates& templates;
    const NpcDefinitions& npc_definitions;
    MobInitPorts& init;
    WorldCorpses& world_corpses;
    // Bind transport/Lua/allocation services to THIS actual zone, not just cells.
    std::function<MobZoneServicesPorts(RuntimeZone&)> bind;
    std::function<std::span<const LinkedMobGroup>(const ConfiguredMap&)> linked;
    std::function<std::uint32_t()> unix_time;
    std::function<void(Npc&)> release_npc;
    std::function<void(RuntimeZone&)> enqueue_wait;
    std::function<ZoneLocalTime()> local_time;
};
// Concrete map/environment/cell/population factory; no registry or pool here.
// Serialized by the SAME root executor. All zones must die while pool is alive;
// this service/source retention must survive the actual pool destruction.
class MobZoneServices final:public ZoneServices,public std::enable_shared_from_this<MobZoneServices> {
public:
    static std::shared_ptr<MobZoneServices> create(MobZoneServicesInputs);
    std::unique_ptr<RuntimeZone> allocate()override;
    bool initialize_map(RuntimeZone&,const ConfiguredMap&)override;
    bool initialize_objects(RuntimeZone&)override;
    void release_objects(RuntimeZone&)override;
    void enqueue_wait(RuntimeZone&)override;
    ZoneLocalTime local_time()override;
private:
    struct Zone;
    explicit MobZoneServices(MobZoneServicesInputs);
    Zone& owned(RuntimeZone&);
    void destroy(Zone&);
    void retain(RuntimeZone&);
    MobZoneServicesInputs in_;
    std::vector<std::unique_ptr<MobPopulation>> regular_;
    std::vector<std::unique_ptr<LinkedMobPopulation>> linked_;
};
}
