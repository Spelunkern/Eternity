#pragma once
#include "eternity/game/money.hpp"
#include "eternity/game/ground_items.hpp"
#include "eternity/game/item_pick.hpp"

namespace eternity::game {
enum class InventoryReceipt {money,merged,stored,returned,rejected};
class InventoryWorld:public MoneyWorld {
public:
    // Capture/lock the current group and recheck membership, as ItemGet does.
    // A nonzero token keeps the captured group's lock until unlock, even if
    // later notices observe a different current membership.
    virtual std::uint32_t lock_inventory_party(Actor&) = 0;
    virtual void unlock_inventory_party(std::uint32_t token) noexcept = 0;
    virtual std::optional<bool> inventory_party_kind(const Actor&) = 0;
    virtual void send_inventory_party(Actor&,const net::Packet&) = 0; // CParty::Send, radius 100.
    virtual void send_inventory_database(const net::Packet&) = 0;
    virtual std::string inventory_item_name(const ItemDefinition&) = 0; // Original bytes, <=31.
    virtual std::string inventory_drop_user_name(Actor&,std::uint32_t id,bool zone_lookup) = 0;
    virtual bool return_inventory_item(Actor&,GroundItem&) = 0; // EnterItem, appearance 1/default lifetime.
};
class ItemPickupWorld:public InventoryWorld,public ItemPickWorld {};
std::uint32_t pickup_money_amount(std::uint32_t amount,std::uint8_t drop_type,std::int32_t amulet,std::int32_t gold_bonus) noexcept;
net::Packet inventory_failure_packet(std::uint8_t reason);
net::Packet inventory_count_packet(std::uint32_t account_id,std::uint8_t bag,std::uint8_t slot,std::uint8_t count);
net::Packet inventory_add_packet(std::uint32_t account_id,std::uint8_t bag,std::uint8_t slot,const GroundItem&);
net::Packet inventory_receipt_packet(std::uint8_t bag,std::uint8_t slot,const PlayerItem&,bool extended);
}
