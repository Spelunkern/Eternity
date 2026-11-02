#pragma once
#include "eternity/game/inventory_replica.hpp"

namespace eternity::game {
// Serialized DB-agent owner for core state and inventory. Money has one authority:
// core() projects the inventory cache's balance, including gem/craft/repair updates.
class CharacterReplica {
public:
    CharacterReplica(PlayerCore core,InventoryReplicaPool& pool):core_(std::move(core)),inventory_(core_.user_id,core_.money,pool) {}
    PlayerCore core() const {auto result=core_;result.money=inventory_.money();return result;}
    InventoryReplica& inventory() noexcept {return inventory_;}
    const InventoryReplica& inventory() const noexcept {return inventory_;}
    InventoryReplicaResult apply(std::span<const std::uint8_t>);
    bool body_changed() const noexcept {return body_changed_;}
    // Core portion of CUser::SaveSuccess 00409560, after complete character commit.
    void core_save_success() noexcept {body_changed_=false;}
private:
    PlayerCore core_;InventoryReplica inventory_;bool body_changed_=false;
};
}
