#pragma once
#include <cstdint>
#include <span>
#include <vector>

namespace eternity::game {
struct ItemQuestLink {std::vector<std::uint16_t> start,end;};
class ItemQuestLinks {
public:
    ItemQuestLinks();
    static ItemQuestLinks decode(std::span<const std::uint8_t>);
    static ItemQuestLinks npc_quest(std::span<const std::uint8_t>);
    const ItemQuestLink* find(std::int32_t type,std::int32_t type_id) const noexcept;
    std::size_t consumed() const noexcept {return consumed_;}
    std::span<const ItemQuestLink> records() const noexcept {return links_;}
private:
    std::vector<ItemQuestLink> links_;
    std::size_t consumed_=0;
};
}
