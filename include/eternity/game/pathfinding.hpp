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
    AStarPath(AStarPath&&) noexcept;
    AStarPath& operator=(AStarPath&&) noexcept;
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
class ChaseTrail {
public:
    void add(const MapDefinition&,MapTile);
    void clear() noexcept {nodes_.clear();}
    std::optional<MapTile> current() const noexcept {return nodes_.empty()?std::nullopt:std::optional<MapTile>(nodes_.back());}
    std::optional<MapTile> pop();
    std::span<const MapTile> nodes() const noexcept {return nodes_;} // Oldest first.
private:
    std::vector<MapTile> nodes_;
};
}
