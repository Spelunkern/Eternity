#include "eternity/game/zone_entry.hpp"
#include <bit>

namespace eternity::game {
namespace {
void append(net::Packet& packet,std::uint32_t value,unsigned int bytes=4) {for(unsigned int i=0;i<bytes;++i) packet.push_back(static_cast<std::uint8_t>(value>>(8*i)));}
}
bool enter_zone(ZoneEntryState& state,const ZoneEntryMap& map,const ZoneEntryPorts& port) {
    if(!port.attach || !port.cell_info || !port.send_view || !port.shape || !port.send_shape || !port.send_speed || !port.weather || !port.obelisks || !port.entered || !port.diagnostic || !port.close) throw std::invalid_argument("Incomplete zone entry services");
    const auto cell=MapGeometry(map.size).cell(state.position.x,state.position.z);
    if(!cell || cell->x>=map.width || cell->z>=map.height) {port.diagnostic();port.close(9);return false;}
    if(map.dungeon==1 && state.vehicle_mode==2) state.vehicle_mode=0;
    state.cell=*cell;state.map=map.id;state.bound=true;port.attach(*cell);
    net::Packet packet{1,2};append(packet,state.character_id);packet.push_back(state.permission<=10?2:1);append(packet,state.direction,2);
    for(auto value:{state.position.x,state.position.y,state.position.z}) append(packet,std::bit_cast<std::uint32_t>(value));
    append(packet,std::bit_cast<std::uint32_t>(state.guild_user_level));append(packet,state.vehicle_mode==3?state.vehicle_partner:0);port.send_view(*cell,std::move(packet));
    if(port.shape()!=0) port.send_shape();
    port.send_speed();port.cell_info(*cell);port.weather();port.obelisks();port.entered();return true;
}
bool enter_zone_actor(RuntimeZone& zone,Actor& actor,ZoneEntryState& state,const ZoneActorEntryServices& services) {
    if(!zone.map || !zone.users || !zone.environment) throw std::logic_error("Zone entry requires initialized map, user grid and environment");
    if(state.character_id!=actor.id()) throw std::invalid_argument("Zone entry actor identity differs");
    if(!services.send_view || !services.cell_info || !services.obelisks || !services.entered || !services.diagnostic || !services.close) throw std::invalid_argument("Incomplete actor zone entry services");
    state.vehicle_mode=actor.appearance().vehicle_mode;state.vehicle_partner=actor.appearance().vehicle_partner;
    const auto& map=*zone.map;const auto geometry=map.definition.geometry();ZoneEntryPorts port;
    port.attach=[&](CellCoordinates cell){actor.appearance().vehicle_mode=state.vehicle_mode;zone.users->attach(actor,cell.x,cell.z,state.position);};port.send_view=services.send_view;port.cell_info=services.cell_info;
    port.shape=[&](){return actor.effective_shape();};port.send_shape=[&](){actor.shape_changed(actor.status());};port.send_speed=[&](){actor.speed_changed(actor.status());};port.weather=[&](){actor.send_self(zone.environment->weather_packet(0));};
    port.obelisks=services.obelisks;port.entered=services.entered;port.diagnostic=services.diagnostic;port.close=services.close;
    return enter_zone(state,{map.definition.size,geometry.cell_count(),geometry.cell_count(),static_cast<std::uint16_t>(map.settings.id),map.settings.dungeon?1U:0U},port);
}
}
