#include "eternity/game/zone_environment.hpp"
#include <cmath>

namespace eternity::game {
namespace {
std::optional<CellCoordinates> object_cell(UserPosition position,std::int32_t count) {
    // MapInit 0041c8a0 validates truncated CELL coordinates, not IsSafePos/world bounds.
    const auto x=std::trunc(static_cast<double>(position.x)/40.0),z=std::trunc(static_cast<double>(position.z)/40.0);
    if(!std::isfinite(x) || !std::isfinite(z) || x<0 || z<0 || x>=count || z>=count) return {};
    return CellCoordinates{static_cast<std::int32_t>(x),static_cast<std::int32_t>(z)};
}
net::Packet door_packet(const ZoneDoor& door) {return {3,4,static_cast<std::uint8_t>(door.id),static_cast<std::uint8_t>(door.open)};}
void close(ZoneDoor& door,std::uint32_t tick,ZoneEnvironmentSink& sink) {
    if(!door.open || door.auto_close==0 || tick<door.next_close) return;
    door.open=false;if(door.cell) sink.send_cell_view(*door.cell,door_packet(door));
}
}
bool ZoneWeather::update(std::uint32_t tick,RandomSource& random) {
    // stMapWeather::Update 004290d0: unsigned deadlines and signed chance/rate comparison.
    if(next_update>tick) return false;
    if(random.next()%100<definition.rate) {
        const bool changed=active==0;active=1;next_update=tick+definition.delay_ms;return changed;
    }
    if(active==1) {active=0;next_update=tick+definition.none_delay_ms;return true;}
    active=0;next_update=tick+definition.delay_ms;return false;
}
ZoneEnvironment ZoneEnvironment::prepare(const ConfiguredMap& map) {
    ZoneEnvironment result;result.weather_.definition=map.settings.weather;
    const auto count=map.definition.geometry().cell_count();
    for(const auto& source:map.definition.doors) if(const auto cell=object_cell(source.position,count)) {
        ZoneDoor door;door.id=static_cast<std::uint32_t>(result.doors_.size());door.position=source.position;door.cell=*cell;
        door.auto_close=source.auto_close;door.close_delay_ms=static_cast<std::uint32_t>(source.close_time)*1000U;result.doors_.push_back(door);
    }
    for(const auto& source:map.definition.others) if(source.type==1) {
        const auto& a=source.box.minimum;const auto& b=source.box.maximum;
        const UserPosition position{static_cast<float>((static_cast<double>(a.x)+b.x)/2.0),static_cast<float>((static_cast<double>(a.y)+b.y)/2.0),static_cast<float>((static_cast<double>(a.z)+b.z)/2.0)};
        if(const auto cell=object_cell(position,count)) {
            ZoneRegion region;region.id=static_cast<std::uint32_t>(result.regions_.size());region.position=position;region.cell=*cell;region.box=source.box;region.type=source.type;
            region.values={source.attribute1,source.attribute2,source.attribute3};result.regions_.push_back(region);
        }
    }
    return result;
}
bool ZoneEnvironment::use_door(std::uint32_t id,std::uint32_t tick,ZoneEnvironmentSink& sink) {
    // CDoor::Use 004b9590. Closing retains next_close; cooldown applies to both directions.
    if(id>=doors_.size()) return false;
    auto& door=doors_[id];if(tick<door.next_use) return false;
    if(door.open) door.open=false;else {door.open=true;if(door.auto_close!=0) door.next_close=tick+door.close_delay_ms;}
    door.next_use=tick+2000U;if(door.cell) sink.send_cell_view(*door.cell,door_packet(door));return true;
}
void ZoneEnvironment::auto_close_door(std::uint32_t id,std::uint32_t tick,ZoneEnvironmentSink& sink) {if(id<doors_.size()) close(doors_[id],tick,sink);}
void ZoneEnvironment::update_doors(std::uint32_t tick,ZoneEnvironmentSink& sink) {for(auto& door:doors_) close(door,tick,sink);}
net::Packet ZoneEnvironment::weather_packet(std::uint8_t mode) const {
    // CZone::SendWeather 004201b0 and UpdateWeather 0041d4d0 send type AND power, five bytes.
    return {0x51,4,mode,weather_.active?static_cast<std::uint8_t>(weather_.definition.state):std::uint8_t{0},weather_.active?static_cast<std::uint8_t>(weather_.definition.power):std::uint8_t{0}};
}
void ZoneEnvironment::update_weather(std::uint32_t tick,RandomSource& random,ZoneEnvironmentSink& sink) {if(weather_.update(tick,random)) sink.send_all(weather_packet(1));}
}
