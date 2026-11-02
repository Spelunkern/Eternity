#pragma once
#include "eternity/game/inventory_replica.hpp"

namespace eternity::game {
struct CachedSavePoint {
    std::int32_t save_type=0;
    std::uint16_t map=65535;
    float x=0,y=0,z=0;
};
class InterfaceReplica {
public:
    explicit InterfaceReplica(std::uint32_t account):account_(account) {}
    bool load(const PlayerSnapshot&);
    bool load_quick_slots(std::span<const QuickSlot>);
    // SQL loader ignores slots >=4; repeated loaded slots overwrite earlier rows.
    bool load_point(std::uint8_t slot,std::uint16_t map,float x,float y,float z);
    InventoryReplicaResult apply(std::span<const std::uint8_t>);
    std::span<const QuickSlot> quick_slots() const noexcept {return {quick_.data(),quick_count_};}
    const std::array<CachedSavePoint,4>& points() const noexcept {return points_;}
    net::Packet quick_slots_packet() const; // DB-agent -> game 0508.
    net::Packet save_points_packet() const; // DB-agent -> game 0d01.
    // SaveSuccess 004098fe clears all four point flags; quick slots have no dirty flag.
    void save_success() noexcept {for(auto& point:points_) point.save_type=0;}
private:
    std::uint32_t account_;std::array<QuickSlot,128> quick_{};std::size_t quick_count_=0;
    std::array<CachedSavePoint,4> points_{};
};
}
