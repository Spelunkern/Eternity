#pragma once
#include "eternity/game/map_config.hpp"
#include "eternity/game/zone_environment.hpp"
#include <memory>

namespace eternity::game {
struct ZoneLocalTime {std::uint16_t month=0,day=0,weekday=0,hour=0;};
std::optional<std::uint32_t> guild_rank_window(const MapSettings&,ZoneLocalTime);
struct ZoneGuildAccess {
    std::uint32_t id=0;
    std::uint8_t ranking_index=255,house_acquired=0,house_enabled=0;
};
bool guild_house_enabled(const ZoneGuildAccess&) noexcept;
struct ZoneUserAccess {
    std::uint8_t permission=255;
    std::uint32_t instance_owner=0,last_rank_entry=0;
    std::optional<std::uint32_t> party_id;
    std::optional<ZoneGuildAccess> guild;
};
struct RuntimeZone {
    virtual ~RuntimeZone() = default; // Concrete world zones own/destruct their object resources.
    std::uint32_t id=0,owner=0,expiry_tick=0;
    const ConfiguredMap* map=nullptr;
    std::unique_ptr<ZoneUsers> users;
    std::optional<ZoneEnvironment> environment;
};
class ZoneServices {
public:
    virtual ~ZoneServices() = default;
    virtual std::unique_ptr<RuntimeZone> allocate() = 0;
    virtual bool initialize_map(RuntimeZone&,const ConfiguredMap&) = 0;
    virtual bool initialize_objects(RuntimeZone&) = 0;
    virtual void release_objects(RuntimeZone&) = 0;
    virtual void enqueue_wait(RuntimeZone&) = 0;
    virtual ZoneLocalTime local_time() = 0;
};
struct ZoneStatistics {
    std::uint32_t house_current=0,rank_current=0,house_peak=0,rank_peak=0,party_current=0,party_peak=0,total_created=0;
};
// Serialized world executor; maps and services must outlive the registry. Required services
// own map objects, their actual initialization and the server's zone queue; no no-op defaults.
class ZoneRegistry {
public:
    ZoneRegistry(const ServerMaps&,ZoneServices&,std::uint32_t serial=0,ZoneStatistics statistics = {});
    bool initialize();
    RuntimeZone* select(std::uint16_t map_id,const ZoneUserAccess&);
    const ZoneStatistics& statistics() const noexcept {return statistics_;}
    std::uint32_t serial() const noexcept {return serial_;}
private:
    struct Node {
        std::unique_ptr<RuntimeZone> permanent;
        std::vector<std::unique_ptr<RuntimeZone>> active,pool;
        std::map<std::uint32_t,RuntimeZone*> owned;
    };
    const ServerMaps& maps_;ZoneServices& services_;std::vector<Node> nodes_;
    std::uint32_t serial_;ZoneStatistics statistics_;bool started_=false,ready_=false;
};
}
