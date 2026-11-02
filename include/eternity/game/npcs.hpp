#pragma once
#include "eternity/game/maps.hpp"
#include <array>
#include <string>

namespace eternity::game {
struct NpcGate {
    std::uint16_t map_id=0;
    UserPosition position;
    std::array<std::uint8_t,256> name{};
    std::uint32_t money=0;
};
struct NpcDefinition {
    // Original stNpc prefix, including its forced name terminator and padding.
    std::array<std::uint8_t,276> record{};
    std::vector<std::uint16_t> start_quests,end_quests;
    std::uint8_t shop_type=0;
    std::vector<std::array<std::uint8_t,2>> shop_items;
    std::array<NpcGate,3> gates{};
    std::uint8_t type() const noexcept {return record[0];}
    std::uint16_t type_id() const noexcept;
    std::int32_t model() const noexcept;
    std::int32_t move_distance() const noexcept;
    std::int32_t move_speed() const noexcept;
    std::int32_t country() const noexcept;
    std::string name() const;
};
class NpcDefinitions {
public:
    // CNpcData::Load 004c9410: thirteen lists at the start of NpcQuest.SData.
    static NpcDefinitions decode(std::span<const std::uint8_t>);
    const NpcDefinition* find(std::int32_t type,std::int32_t type_id) const noexcept;
    std::size_t count(std::int32_t type) const noexcept;
    std::size_t consumed() const noexcept {return consumed_;}
private:
    std::array<std::vector<NpcDefinition>,13> lists_;
    std::size_t consumed_=0;
};
}
