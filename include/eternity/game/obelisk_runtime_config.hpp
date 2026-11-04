#pragma once
#include "eternity/game/obelisk_runtime.hpp"
namespace eternity::game {
// Parsed stObeliskZone/stObelisk inputs. Text parsing is a separate provider.
struct ObeliskRuntimeSpawnInput {
    UserPosition center;float radius=0;std::array<std::int32_t,3> mobs{};std::int32_t count=0;
};
struct ObeliskRuntimeInput {
    std::uint32_t base_id=0;std::int32_t country=0;UserPosition center;
    std::array<std::int32_t,3> mobs{}; // File order: neutral, light, fury.
    std::vector<ObeliskRuntimeSpawnInput> spawns;
};
struct ObeliskRuntimeZoneInput {std::int32_t map=0;std::vector<ObeliskRuntimeInput> records;};
struct ObeliskRuntimeConfigurationState {
    std::uint32_t next_id=0;std::int32_t total=0;
    std::array<std::int32_t,2> kill_count_add{},maximum_kill_count{};
};
class ObeliskRuntimeDefinitionOwner {
public:
    virtual ~ObeliskRuntimeDefinitionOwner()=default;
    // Required mutation of shared stMOBInfo::bObelisk, not a private copy.
    virtual void mark_obelisk(const MobRuntimeTemplate&)=0;
};
class ObeliskRuntimeConfiguration {
public:
    ObeliskRuntimeConfiguration()=default;
    ObeliskRuntimeConfiguration(const ObeliskRuntimeConfiguration&)=delete;
    ObeliskRuntimeConfiguration& operator=(const ObeliskRuntimeConfiguration&)=delete;
    ObeliskRuntimeConfiguration(ObeliskRuntimeConfiguration&&)=default;
    ObeliskRuntimeConfiguration& operator=(ObeliskRuntimeConfiguration&&)=default;
    // LoadObelisk 00427300 materialization only. Definitions and this owner must
    // outlive all mobs, death/removal adapters and portal bindings.
    static ObeliskRuntimeConfiguration bind(std::span<const ObeliskRuntimeZoneInput>,
        std::span<const std::int32_t> map_sizes,const MobRuntimeTemplates&,
        ObeliskRuntimeDefinitionOwner&,ObeliskRuntimeConfigurationState&);
    std::span<ObeliskRuntime> for_map(std::size_t id) noexcept;
private:
    std::vector<std::vector<ObeliskRuntime>> maps_;
};
}
