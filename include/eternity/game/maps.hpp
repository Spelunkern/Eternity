#pragma once
#include "eternity/game/zone_users.hpp"
#include <filesystem>
#include <optional>
#include <span>

namespace eternity::game {
struct CellCoordinates {std::int32_t x=0,z=0;};
struct CellViewBounds {float minimum_x=0,maximum_x=0,minimum_z=0,maximum_z=0;bool contains(float x,float z) const noexcept;};
class MapGeometry {
public:
    explicit MapGeometry(std::int32_t size);
    std::int32_t size() const noexcept {return size_;}
    std::int32_t cell_count() const noexcept {return size_/40+1;}
    bool safe_position(float x,float z) const noexcept;
    std::optional<CellCoordinates> cell(float x,float z) const noexcept;
    std::optional<CellCoordinates> cell_after_move(CellCoordinates previous,float x,float z) const;
    CellViewBounds view(CellCoordinates) const;
private:
    std::int32_t size_;
};
struct MapBox {UserPosition minimum,maximum;};
struct MapDoor {UserPosition position;std::int32_t auto_close=0,close_time=0;};
struct MapMobSpawn {std::int32_t mob_id=0,count=0;};
struct MapMobArea {MapBox box;std::int32_t delta_x=1,delta_z=1;std::vector<MapMobSpawn> spawns;};
struct MapNpcPosition {UserPosition position;float angle=0;};
struct MapNpc {std::int32_t type=0,type_id=0;std::vector<MapNpcPosition> positions;};
struct MapPortal {
    std::int32_t id=-1;UserPosition position;std::int32_t base_country=0,country=0,minimum_level=0,maximum_level=0,map_id=0;
    UserPosition destination;
};
struct MapBind {std::int32_t country=0,attribute=0;std::vector<MapBox> boxes;};
struct MapOther {MapBox box;std::int32_t type=0;std::uint8_t attribute1=0,attribute2=0;std::uint16_t attribute3=0;};
struct MapDefinition {
    std::int32_t size=0;
    std::vector<std::uint8_t> tiles;
    std::vector<MapDoor> doors;
    std::vector<UserPosition> ladders;
    std::vector<MapMobArea> mobs;
    std::vector<MapNpc> npcs;
    std::vector<MapPortal> portals;
    std::vector<MapBind> binds;
    std::vector<MapOther> others;
    std::size_t consumed=0;
    static MapDefinition decode(std::span<const std::uint8_t>);
    static MapDefinition load(const std::filesystem::path&);
    MapGeometry geometry() const {return MapGeometry(size);}
};
}
