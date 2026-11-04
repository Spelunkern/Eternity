#include "eternity/game/obelisk_runtime_maps.hpp"
#include <stdexcept>
namespace eternity::game {
ObeliskRuntimeMaps::ObeliskRuntimeMaps(ServerMaps& maps,const MobRuntimeTemplates& templates,std::string_view ini,ObeliskRuntimeDefinitionRecords::Access access,ObeliskRuntimeConfigurationState& state,ObeliskRuntimeMapsServices& services):maps_(maps),services_(services) {
    std::vector<std::int32_t> sizes;for(const auto& m:maps_.all()) sizes.push_back(m.definition.size);
    ObeliskRuntimeDefinitionRecords owner(std::move(access));
    ordinary_=ObeliskRuntimeConfiguration::bind(parse_obelisk_runtime_configuration(ini),sizes,templates,owner,state);
    runtime_.resize(maps_.size());
    std::vector<std::vector<ObeliskRuntimePortalTarget>> targets(maps_.size());std::vector<ObeliskRuntimePortalMap> bindings;
    for(std::size_t i=0;i<maps_.size();++i) {
        const auto& m=map(i);auto& r=runtime_[i];r.id=static_cast<std::uint16_t>(i);r.size=m.definition.size;r.blocked_tiles=m.definition.tiles;
        for(std::size_t j=0;j<m.definition.portals.size();++j) targets[i].push_back({m.definition.portals[j].id,maps_.mutable_portal_country(static_cast<std::uint16_t>(i),j)});
        bindings.push_back({targets[i],ordinary_.for_map(i),services_.primary_zone(m)!=nullptr});
    }
    portals_=ObeliskRuntimePortalConfiguration::bind(parse_obelisk_runtime_portal_configuration(ini),bindings,state.next_id);
    for(std::size_t i=0;i<portals_.links().size();++i) {
        auto& link=portals_.links()[i];const auto id=static_cast<std::size_t>(portals_.target_map(i));const auto& m=map(id);const MapPortal* target=nullptr;
        for(std::size_t j=0;j<m.definition.portals.size();++j) if(maps_.mutable_portal_country(static_cast<std::uint16_t>(id),j)==link.country) {target=&m.definition.portals[j];break;}
        if(!target) throw std::logic_error("Portal binding did not retain actual ServerMaps country");
        scenes_.push_back({&m,&link,&target->position});
    }
    refresh();
}
const ConfiguredMap& ObeliskRuntimeMaps::map(std::size_t i) const {
    if(i>=maps_.size()) throw std::out_of_range("Obelisk map index outside ServerMaps");
    return maps_.all()[i];
}
void ObeliskRuntimeMaps::refresh() {
    for(std::size_t i=0;i<runtime_.size();++i) runtime_[i].zone_present=services_.primary_zone(map(i))!=nullptr;
    for(std::size_t i=0;i<portals_.links().size();++i) portals_.links()[i].zone_present=services_.primary_zone(map(static_cast<std::size_t>(portals_.target_map(i))))!=nullptr;
}
std::span<ObeliskRuntime> ObeliskRuntimeMaps::records(std::size_t i) {static_cast<void>(map(i));return ordinary_.for_map(i);}
ObeliskRuntimeMap& ObeliskRuntimeMaps::runtime_map(std::size_t i) {static_cast<void>(map(i));refresh();return runtime_.at(i);}
void ObeliskRuntimeMaps::restore(std::uint32_t id,std::uint32_t country,std::uint32_t change,ObeliskRuntimeOwnershipClock& clock) {
    std::vector<std::span<ObeliskRuntime>> maps;for(std::size_t i=0;i<maps_.size();++i) maps.push_back(ordinary_.for_map(i));
    std::vector<ObeliskRuntimePortalOwnership> portals;for(auto& p:portals_.links()) portals.push_back({p.base_id,p.country,&p.open});
    restore_obelisk_ownership(maps,portals,id,country,change,clock);refresh();
}
void ObeliskRuntimeMaps::update_portals() {refresh();update_obelisk_portal_links(portals_.links(),*this);}
void ObeliskRuntimeMaps::reset(std::size_t i,const std::function<std::uint32_t()>& tick,ObeliskRuntimeWorld& world) {auto& m=runtime_map(i);reset_obelisks(m,records(i),tick,world);}
void ObeliskRuntimeMaps::send_user(std::size_t i,ObeliskRuntimeUserPublication& user) {refresh();publish_obelisk_user_list(&map(i),records(i),scenes_,user);}
void ObeliskRuntimeMaps::persist() {
    refresh();std::vector<std::span<const ObeliskRuntime>> maps;for(std::size_t i=0;i<maps_.size();++i) maps.push_back(ordinary_.for_map(i));
    publish_world_obelisk_database_lists(portals_.links(),maps,*this);
}
void ObeliskRuntimeMaps::publish_change(std::size_t id,std::size_t index) {
    struct Route final:ObeliskRuntimePublication {
        ObeliskRuntimeMaps& owner;ObeliskRuntimeMapsServices& services;const ConfiguredMap& map;std::size_t id;
        Route(ObeliskRuntimeMaps& o,ObeliskRuntimeMapsServices& s,const ConfiguredMap& m,std::size_t i):owner(o),services(s),map(m),id(i) {}
        void zone_packet(const net::Packet& p) override {if(auto* z=services.primary_zone(map)) services.map_broadcast(map,*z,p);static_cast<void>(owner.runtime_map(id));}
        void database_packet(const net::Packet& p) override {owner.database(p);}
    } route(*this,services_,map(id),id);
    auto values=records(id);if(index>=values.size()) throw std::out_of_range("Obelisk change index outside retained map");
    publish_obelisk_change(runtime_map(id),values[index],route);
}
void ObeliskRuntimeMaps::database(const net::Packet& p) {services_.database(p);refresh();}
void ObeliskRuntimeMaps::database_packet(const net::Packet& p) {database(p);}
void ObeliskRuntimeMaps::map_broadcast(std::size_t i,const net::Packet& p) {const auto& m=map(static_cast<std::size_t>(portals_.target_map(i)));if(auto* zone=services_.primary_zone(m)) services_.map_broadcast(m,*zone,p);refresh();}
void ObeliskRuntimeMaps::broadcast(const net::Packet& p) {services_.world_broadcast(p);refresh();}
void ObeliskRuntimeMaps::zone_packet(const net::Packet&) {throw std::logic_error("Obelisk map publication requires an explicit retained map/zone route");}
}
