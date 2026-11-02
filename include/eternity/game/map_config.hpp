#pragma once
#include "eternity/game/maps.hpp"
#include <array>
#include <string>
#include <string_view>

namespace eternity::game {
struct MapLocation {std::int32_t map=-1;UserPosition position;};
struct MapOpenTime {std::uint16_t day=65535,hour=0,keep_hours=0;};
struct MapWeather {
    std::int32_t state=0,power=2,rate=30;
    std::uint32_t delay_ms=180000,none_delay_ms=1800000;
};
struct MapSettings {
    std::int32_t id=0,war_type=0,create_type=0; // CreateType N/S/F/P/G/R/H -> 0..6.
    bool dungeon=false,market=false;
    std::string filename;
    std::array<MapLocation,2> rebirth;
    MapWeather weather;
    std::array<MapOpenTime,7> open_times;
    std::uint32_t expire_ms=30000;
    std::int32_t create_minimum_users=0,enter_maximum_users=30;
};
struct MapConfiguration {
    std::array<std::vector<MapLocation>,4> starts;
    std::vector<MapSettings> maps;
    static MapConfiguration decode(std::string_view);
    static MapConfiguration load(const std::filesystem::path&);
};
struct ConfiguredMap {MapSettings settings;MapDefinition definition;};
class ServerMaps {
public:
    // Load exactly BASE.MapCount configured files, including file aliases. Runtime zones,
    // obelisk links and spawned objects are initialized by the world owner afterward.
    static ServerMaps load(const MapConfiguration&,const std::filesystem::path& directory);
    const ConfiguredMap* find(std::uint16_t id) const noexcept;
    std::size_t size() const noexcept {return maps_.size();}
    std::span<const ConfiguredMap> all() const noexcept {return maps_;}
private:
    std::vector<ConfiguredMap> maps_;
};
}
