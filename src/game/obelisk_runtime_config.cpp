#include "eternity/game/obelisk_runtime_config.hpp"
#include <bit>
#include <cmath>
#include <stdexcept>
namespace eternity::game {
namespace {
std::int32_t increment(std::int32_t n) {return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(n)+1U);}
std::int32_t radius_twice(float radius) {
    // Original first truncates radius to int, converts back to float, doubles,
    // then truncates again. This differs from trunc(2*radius).
    const auto truncated=std::trunc(static_cast<double>(radius));
    const auto first=!std::isfinite(truncated)||truncated<INT32_MIN||truncated>INT32_MAX?INT32_MIN:static_cast<std::int32_t>(truncated);
    const auto doubled=static_cast<double>(static_cast<float>(first))*2;
    return doubled<INT32_MIN||doubled>INT32_MAX?INT32_MIN:static_cast<std::int32_t>(doubled);
}
std::array<const MobRuntimeTemplate*,3> definitions(const std::array<std::int32_t,3>& ids,const MobRuntimeTemplates& catalog) {
    return {catalog.find(static_cast<std::uint32_t>(ids[1])),catalog.find(static_cast<std::uint32_t>(ids[2])),catalog.find(static_cast<std::uint32_t>(ids[0]))};
}
}
ObeliskRuntimeConfiguration ObeliskRuntimeConfiguration::bind(std::span<const ObeliskRuntimeZoneInput> zones,std::span<const std::int32_t> sizes,const MobRuntimeTemplates& catalog,ObeliskRuntimeDefinitionOwner& owner,ObeliskRuntimeConfigurationState& state) {
    ObeliskRuntimeConfiguration result;result.maps_.resize(sizes.size());
    for(const auto& zone:zones) {
        if(zone.map<0||static_cast<std::size_t>(zone.map)>=sizes.size()||zone.records.empty()) continue;
        auto& records=result.maps_[static_cast<std::size_t>(zone.map)];records.clear();records.resize(zone.records.size());
        for(std::size_t i=0;i<records.size();++i) {
            const auto& raw=zone.records[i];auto& o=records[i];
            o.id=state.next_id++;o.base_id=raw.base_id;o.base_country=raw.country==1?0:raw.country==2?1:2;
            o.country=o.base_country;o.last_destroy_country=o.base_country;o.center=raw.center;o.definitions=definitions(raw.mobs,catalog);
            for(const auto* definition:o.definitions) if(definition) owner.mark_obelisk(*definition);
            const auto size=static_cast<float>(sizes[static_cast<std::size_t>(zone.map)]);
            if(o.center.x<0||o.center.x>=size||o.center.z<0||o.center.z>=size) {o.definitions={};continue;}
            state.total=increment(state.total);
            if(raw.spawns.empty()) continue; // Original leaves principal respawn zero without groups.
            o.respawn.type=static_cast<std::int32_t>(i);o.respawn.count=1;o.respawn.obelisk=true;
            o.spawns.resize(raw.spawns.size());
            for(std::size_t j=0;j<o.spawns.size();++j) {
                const auto& source=raw.spawns[j];auto& spawn=o.spawns[j];
                if(source.count<0) throw std::invalid_argument("Negative original obelisk member allocation is unsupported");
                spawn.center=source.center;spawn.radius=source.radius;spawn.radius_twice=radius_twice(source.radius);spawn.count=source.count;
                spawn.definitions=definitions(source.mobs,catalog);spawn.respawn.count=source.count;spawn.respawn.obelisk_source=&spawn;
                spawn.members.resize(static_cast<std::size_t>(source.count));
            }
        }
    }
    if(state.total>0) for(std::size_t i=0;i<2;++i) state.kill_count_add[i]=increment(state.maximum_kill_count[i]/state.total);
    return result;
}
std::span<ObeliskRuntime> ObeliskRuntimeConfiguration::for_map(std::size_t id) noexcept {return id<maps_.size()?std::span<ObeliskRuntime>(maps_[id]):std::span<ObeliskRuntime>{};}
}
