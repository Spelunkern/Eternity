#include "eternity/game/item_quests.hpp"
#include "eternity/game/npcs.hpp"
#include <stdexcept>

namespace eternity::game {
ItemQuestLinks::ItemQuestLinks():links_(65536) {}
ItemQuestLinks ItemQuestLinks::decode(std::span<const std::uint8_t> data) {
    // ps_game.exe CItemQuestData::Load 004c9d90: 256x256 pairs of ordered WORD lists.
    ItemQuestLinks result;std::size_t offset=0;
    const auto list=[&](std::vector<std::uint16_t>& target) {
        if(data.size()-offset<4) throw std::runtime_error("Truncated original item quest link count");
        std::uint32_t count=0;for(unsigned int i=0;i<4;++i) count|=static_cast<std::uint32_t>(data[offset++])<<(8*i);
        if(count>(data.size()-offset)/2) throw std::runtime_error("Truncated original item quest link IDs");
        target.reserve(count);for(std::uint32_t i=0;i<count;++i) {target.push_back(static_cast<std::uint16_t>(data[offset] | (static_cast<std::uint16_t>(data[offset+1])<<8)));offset+=2;}
    };
    for(auto& link:result.links_) {list(link.start);list(link.end);}result.consumed_=offset;return result;
}
ItemQuestLinks ItemQuestLinks::npc_quest(std::span<const std::uint8_t> data) {
    const auto npcs=NpcDefinitions::decode(data);auto result=decode(data.subspan(npcs.consumed()));result.consumed_+=npcs.consumed();return result;
}
const ItemQuestLink* ItemQuestLinks::find(std::int32_t type,std::int32_t type_id) const noexcept {
    // GetQuestLink 004152f0 returns a nonnull empty record for valid empty slots.
    return type>0 && type<256 && type_id>0 && type_id<256?&links_[static_cast<std::size_t>(type)*256+static_cast<std::size_t>(type_id)]:nullptr;
}
}
