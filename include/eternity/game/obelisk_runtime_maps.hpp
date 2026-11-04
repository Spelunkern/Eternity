#pragma once
#include "eternity/game/map_config.hpp"
#include "eternity/game/obelisk_runtime_config.hpp"
#include "eternity/game/obelisk_runtime_definition_records.hpp"
#include "eternity/game/obelisk_runtime_portal_config.hpp"
#include "eternity/game/obelisk_runtime_ownership.hpp"
#include "eternity/game/obelisk_runtime_user_list.hpp"
#include "eternity/game/obelisk_runtime_world_publication.hpp"
#include <memory>
namespace eternity::game {
class ZoneMobs;
class ObeliskRuntimeMapsServices {
public:
    virtual ~ObeliskRuntimeMapsServices()=default;
    // Actual primary map-zone owner, not an instance-zone lookup or cached flag.
    // Query is passive; service/map/zone lifetimes are externally serialized.
    virtual ZoneMobs* primary_zone(const ConfiguredMap&) const noexcept=0;
    virtual void database(const net::Packet&)=0;
    virtual void world_broadcast(const net::Packet&)=0;
    virtual void map_broadcast(const ConfiguredMap&,ZoneMobs&,const net::Packet&)=0;
};
// Retains only views into actual ServerMaps/catalog/zone owners. All borrowed
// owners and published definitions outlive this nonmovable adapter and its mobs.
class ObeliskRuntimeMaps final:public ObeliskRuntimePortalWorld,public ObeliskRuntimePublication {
public:
    ObeliskRuntimeMaps(ServerMaps&,const MobRuntimeTemplates&,std::string_view ini,
        ObeliskRuntimeDefinitionRecords::Access,ObeliskRuntimeConfigurationState&,ObeliskRuntimeMapsServices&);
    ObeliskRuntimeMaps(const ObeliskRuntimeMaps&)=delete;
    ObeliskRuntimeMaps& operator=(const ObeliskRuntimeMaps&)=delete;
    std::span<ObeliskRuntime> records(std::size_t);
    ObeliskRuntimeMap& runtime_map(std::size_t);
    std::span<ObeliskRuntimePortalLink> portals() noexcept {return portals_.links();}
    void restore(std::uint32_t base_id,std::uint32_t country,std::uint32_t change,ObeliskRuntimeOwnershipClock&);
    void update_portals();
    void reset(std::size_t,const std::function<std::uint32_t()>&,ObeliskRuntimeWorld&);
    void send_user(std::size_t,ObeliskRuntimeUserPublication&);
    void persist();
    void publish_change(std::size_t map,std::size_t record);
    void database(const net::Packet&) override;
    void database_packet(const net::Packet&) override;
    void map_broadcast(std::size_t,const net::Packet&) override;
    void broadcast(const net::Packet&) override;
    void zone_packet(const net::Packet&) override; // No implicit current map.
private:
    void refresh();
    const ConfiguredMap& map(std::size_t) const;
    ServerMaps& maps_;ObeliskRuntimeMapsServices& services_;
    ObeliskRuntimeConfiguration ordinary_;
    ObeliskRuntimePortalConfiguration portals_;
    std::vector<ObeliskRuntimeMap> runtime_;
    std::vector<ObeliskRuntimePortalScene> scenes_;
};
// FullCatalog is the existing canonical full catalog (mutable_mob_record API).
// Runtime templates are its existing admitted Init projection, not rebuilt here.
template<class FullCatalog>
std::unique_ptr<ObeliskRuntimeMaps> bind_obelisk_runtime_maps(ServerMaps& maps,
    const MobRuntimeTemplates& templates,FullCatalog& catalog,std::string_view ini,
    ObeliskRuntimeConfigurationState& state,ObeliskRuntimeMapsServices& services) {
    return std::make_unique<ObeliskRuntimeMaps>(maps,templates,ini,
        [&catalog](std::uint32_t id){return catalog.mutable_mob_record(id);},state,services);
}
}
