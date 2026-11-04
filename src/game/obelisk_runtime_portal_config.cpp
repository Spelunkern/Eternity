#include "eternity/game/obelisk_runtime_portal_config.hpp"
#include <stdexcept>
namespace eternity::game {
ObeliskRuntimePortalConfiguration ObeliskRuntimePortalConfiguration::bind(std::span<const ObeliskRuntimePortalInput> inputs,std::span<const ObeliskRuntimePortalMap> maps,std::uint32_t& next_id) {
    ObeliskRuntimePortalConfiguration result;result.links_.reserve(inputs.size());result.maps_.reserve(inputs.size());
    for(const auto& input:inputs) {
        if(input.map<0||static_cast<std::size_t>(input.map)>=maps.size()) continue;
        const auto& map=maps[static_cast<std::size_t>(input.map)];const ObeliskRuntimePortalTarget* target=nullptr;
        for(const auto& portal:map.portals) if(portal.id==input.portal) {target=&portal;break;}
        if(!target) continue;
        if(!target->country) throw std::invalid_argument("Configured portal requires retained country storage");
        ObeliskRuntimePortalLink link;link.id=next_id++;link.base_id=input.base_id;link.portal_id=static_cast<std::uint16_t>(target->id);link.country=target->country;link.zone_present=map.zone_present;
        for(std::size_t side=0;side<2;++side) for(const auto& ref:input.references[side]) {
            if(ref.map<0||static_cast<std::size_t>(ref.map)>=maps.size()||ref.ordinal<1) continue;
            const auto records=maps[static_cast<std::size_t>(ref.map)].obelisks;
            const auto index=static_cast<std::size_t>(ref.ordinal-1);
            if(index>records.size()||!records.data()) continue;
            if(index==records.size()) throw std::out_of_range("Original one-past portal obelisk reference is unsupported");
            link.links[side*4+static_cast<std::size_t>(link.counts[side]++)]=&records[index];
        }
        if(link.counts[0]||link.counts[1]) {result.links_.push_back(link);result.maps_.push_back(input.map);}
    }
    return result;
}
std::int32_t ObeliskRuntimePortalConfiguration::target_map(std::size_t i) const {return maps_.at(i);}
}
