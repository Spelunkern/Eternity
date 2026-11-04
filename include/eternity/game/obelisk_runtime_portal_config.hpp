#pragma once
#include "eternity/game/obelisk_runtime_portal.hpp"
namespace eternity::game {
struct ObeliskRuntimePortalReference {std::int32_t map=0,ordinal=0;};
struct ObeliskRuntimePortalInput {
    std::uint32_t base_id=0;std::int32_t map=0,portal=0;
    std::array<std::array<ObeliskRuntimePortalReference,4>,2> references{};
};
struct ObeliskRuntimePortalTarget {std::int32_t id=0;std::int32_t* country=nullptr;};
struct ObeliskRuntimePortalMap {
    std::span<const ObeliskRuntimePortalTarget> portals;
    std::span<const ObeliskRuntime> obelisks;
    bool zone_present=false;
};
std::vector<ObeliskRuntimePortalInput> parse_obelisk_runtime_portal_configuration(std::string_view);
class ObeliskRuntimePortalConfiguration {
public:
    // LoadObelisk 00427300 portal materialization. All map/target/obelisk
    // backing storage outlives this configuration and every nested callback.
    static ObeliskRuntimePortalConfiguration bind(std::span<const ObeliskRuntimePortalInput>,
        std::span<const ObeliskRuntimePortalMap>,std::uint32_t& next_id);
    std::span<ObeliskRuntimePortalLink> links() noexcept {return links_;}
    std::int32_t target_map(std::size_t index) const;
private:
    std::vector<ObeliskRuntimePortalLink> links_;
    std::vector<std::int32_t> maps_;
};
}
