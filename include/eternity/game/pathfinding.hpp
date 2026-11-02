#pragma once
#include "eternity/game/maps.hpp"
#include <memory>

namespace eternity::game {
struct PathScanBounds {std::int32_t minimum_x=0,minimum_z=0,maximum_x=0,maximum_z=0;};
// ps_game.exe AStar 0048eda0. Serialized owner; preserves original squared
// heuristic, neighbor/tie order, search window and path compression.
class AStarPath {
public:
    AStarPath();
    ~AStarPath();
    AStarPath(const AStarPath&)=delete;
    AStarPath& operator=(const AStarPath&)=delete;
    bool find(const MapDefinition&,MapTile start,MapTile goal);
    void reset_path(); // Original ResetPath does not clear the found flag.
    void reset();
    void set_path(MapTile);
    bool found() const noexcept;
    bool last() const noexcept;
    std::optional<MapTile> current() const noexcept;
    void advance();
    PathScanBounds bounds() const noexcept;
    std::vector<MapTile> nodes() const; // Full compressed chain, including its start.
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
}
