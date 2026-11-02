#pragma once
#include "eternity/game/ground_items.hpp"
#include "eternity/game/item_quests.hpp"

namespace eternity::game {
class Actor;
class ItemPickClock {
public:
    virtual ~ItemPickClock() = default;
    virtual std::uint32_t pick_tick_ms() = 0;
};
class ItemPickWorld:public ItemPickClock {
public:
    virtual std::uint32_t pick_party_id(Actor&) = 0;
    virtual UserPosition pick_position(const Actor&) = 0;
    virtual bool pick_quest_active(Actor&,std::uint16_t id) = 0;
};
bool ground_item_pick_enabled(const GroundItem&,std::uint32_t user_id,std::uint32_t party_id,bool checks,ItemPickClock&);
}
