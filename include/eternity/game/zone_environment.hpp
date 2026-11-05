#pragma once
#include "eternity/game/map_config.hpp"
#include "eternity/game/random.hpp"
#include "eternity/net/framing.hpp"
#include <functional>
#include <map>

namespace eternity::game {
struct ZoneDoor {
    std::uint32_t id=0;UserPosition position;
    std::optional<CellCoordinates> cell;
    bool open=false;
    std::int32_t auto_close=0;
    std::uint32_t close_delay_ms=0,next_close=0,next_use=0;
};
struct ZoneRegion {
    std::uint32_t id=0;UserPosition position;CellCoordinates cell;MapBox box;
    std::int32_t type=0;std::array<std::int32_t,3> values{};
};
struct ZoneWeather {
    MapWeather definition;
    std::uint8_t active=0;
    std::uint32_t next_update=0;
    bool update(std::uint32_t tick,RandomSource&);
};
class ZoneEnvironmentSink {
public:
    virtual ~ZoneEnvironmentSink() = default;
    virtual void send_cell_view(CellCoordinates,const net::Packet&) = 0;
    virtual void send_all(const net::Packet&) = 0;
};
// Environment portion of MapInit; NPC, mob, obelisk and linked-boss initialization
// remain separate required parts of preparing a complete runtime zone.
class ZoneEnvironment {
public:
    static ZoneEnvironment prepare(const ConfiguredMap&);
    std::vector<ZoneDoor>& doors() noexcept {return doors_;}
    const std::vector<ZoneDoor>& doors() const noexcept {return doors_;}
    const std::vector<ZoneRegion>& regions() const noexcept {return regions_;}
    ZoneWeather& weather() noexcept {return weather_;}
    const ZoneWeather& weather() const noexcept {return weather_;}
    bool use_door(std::uint32_t id,std::uint32_t tick,ZoneEnvironmentSink&);
    void auto_close_door(std::uint32_t id,std::uint32_t tick,ZoneEnvironmentSink&);
    void update_doors(std::uint32_t tick,ZoneEnvironmentSink&);
    void update_weather(std::uint32_t tick,RandomSource&,ZoneEnvironmentSink&);
    net::Packet weather_packet(std::uint8_t mode) const;
    // CCell::SendCellInfoEnter door block. Serialized access; door vector and
    // cell membership must not be structurally mutated during callbacks.
    void send_cell_doors(CellCoordinates,const std::function<void(const net::Packet&)>&);
private:
    std::vector<ZoneDoor> doors_;
    std::vector<ZoneRegion> regions_;
    ZoneWeather weather_;
    std::map<std::pair<std::int32_t,std::int32_t>,std::size_t> door_cursors_;
};
}
