#pragma once
#include "eternity/game/mob_population.hpp"

namespace eternity::game {
struct LinkedMobSpawn {
    UserPosition center;float radius=0;std::int32_t radius_twice=1,count=0;
    std::array<std::int32_t,16> type_ids{};
    std::array<const MobRuntimeTemplate*,16> definitions{};
    MobRespawn respawn;
    std::array<MobRuntime*,16> members{};
    std::int32_t randomized=0;std::uint32_t minimum_minutes=0,random_minutes=0;
    std::array<UserPosition,3> random_centers{};
    void reset_members(); // stMapBothMob::Reset 0041b3d0, first count slots only.
    void add(MobRuntime*); // AddMob 0041b3f0: first empty, duplicates allowed.
    void remove(MobRuntime*); // RemoveMob 0041d730: first match only.
};
struct LinkedMobGroup {
    std::uint32_t id=0;std::int32_t change_count=0;std::uint32_t respawn_time=0;
    std::int32_t portal_map=0,portal_id=0;const MapPortal* portal=nullptr;
    std::int32_t spawn_count=0;LinkedMobSpawn leader;
    std::array<LinkedMobSpawn,32> spawns{};std::int32_t current_index=0;
};
class LinkedMobWorld {
public:
    virtual ~LinkedMobWorld()=default;
    virtual void remove_by_id(std::uint32_t)=0;
    // Write the shared portal country, then the current zone's instance table.
    virtual void set_portal_country(const MapPortal&,std::int32_t)=0;
};
// Per-zone stMapBoth copies. Configuration/SQL binding is supplied by the map
// owner; this does not substitute a loader or an AI/tick provider.
class LinkedMobPopulation {
public:
    explicit LinkedMobPopulation(std::span<const LinkedMobGroup>);
    LinkedMobPopulation(const LinkedMobPopulation&)=delete;LinkedMobPopulation& operator=(const LinkedMobPopulation&)=delete;
    std::span<LinkedMobGroup> groups() noexcept {return groups_;}
    std::span<const LinkedMobGroup> groups() const noexcept {return groups_;}
    // MapInitBothMob 0041b470, called after regular population initialization.
    bool initialize(const MapDefinition&,ZoneMobs&,MobPool&,MobInitPorts&,const std::function<std::uint32_t()>& unix_time);
    // BothMobDestroy 0041bad0 / BothMobRespawn 0041be50 / UpdateBothMobRespawn 0041b870.
    // Death accounting is owned by the caller. Required removal/portal owners
    // may inspect records but must not resize group storage.
    void destroyed(std::size_t,const MapDefinition&,ZoneMobs&,MobPool&,MobInitPorts&,LinkedMobWorld&);
    void respawn(std::size_t,const MapDefinition&,ZoneMobs&,MobPool&,MobInitPorts&,LinkedMobWorld&);
    void update(const MapDefinition&,ZoneMobs&,MobPool&,MobInitPorts&,std::uint32_t now,LinkedMobWorld&);
private:
    std::vector<LinkedMobGroup> groups_;
};
}
