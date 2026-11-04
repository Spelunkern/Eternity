#pragma once
#include "eternity/game/inventory.hpp"
#include <span>

namespace eternity::game {
class GroundItemPool;
// Required live owners for the original handler prefix. Callbacks are synchronous;
// the packet, Actor, zone, pool and services must survive both callbacks.
class VerticalInventoryActions {
public:
    virtual ~VerticalInventoryActions() = default;
    virtual void cancel_action_exchange(Actor&) = 0; // CUser::CancleActionExc
    virtual void end_shop(Actor&) = 0;              // stMyShop::Ended
    virtual bool pickup_checks() = 0; // Pure read of retained ps_game.exe 00541af8 setting.
};
enum class VerticalInventoryRequest {unhandled, malformed, ignored, pickup};
struct VerticalInventoryResult {
    VerticalInventoryRequest request;
    std::optional<InventoryReceipt> receipt;
};
// ps_game.exe CUser::PacketMaininterface 00468420 / 004686c9..004686f2.
// Only opcode 0205 is owned here. Framing policy belongs to the caller.
// The pickup-check setting must remain stable for the synchronous pickup.
VerticalInventoryResult vertical_inventory_request(Actor&,std::span<const std::uint8_t>,
    ZoneItems&,const ItemQuestLinks&,ItemPickupWorld&,GroundItemPool&,
    VerticalInventoryActions&);
}
