#pragma once
#include "eternity/game/players.hpp"
#include <functional>
#include <memory>
#include <mutex>

namespace eternity::game {
struct CachedInventoryItem {
    PlayerItem item;
    std::array<std::uint8_t,21> craft_storage{};
    std::int32_t save_type=0,saved_type=0; // ps_dbAgent CItem+8/+c; 0 unchanged, 1 insert, 2 update.
};
struct InventoryReplicaPoolStatistics {std::uint32_t attempts=0;std::size_t available=0,stranded=0;};
class InventoryReplicaPool {
public:
    using Allocator=std::function<std::unique_ptr<CachedInventoryItem>()>;
    explicit InventoryReplicaPool(Allocator allocator={}):allocator_(std::move(allocator)) {}
    std::unique_ptr<CachedInventoryItem> allocate();
    void release(std::unique_ptr<CachedInventoryItem>);
    void strand(std::unique_ptr<CachedInventoryItem>); // Original reset commands clear pointers without recycling.
    InventoryReplicaPoolStatistics statistics() const;
private:
    mutable std::mutex mutex_;Allocator allocator_;std::uint32_t attempts_=0;
    std::vector<std::unique_ptr<CachedInventoryItem>> available_,stranded_;
};
enum class InventoryReplicaResult {applied,unsupported,rejected,disconnect};
// Serialized DB-agent cache. Billing/guild transfer packets have separate owners;
// only owned inventory/bank commands are accepted here. SQL saving is separate.
// The pool must outlive this cache. Inventory has 6 bags of 24 slots; bank has 240.
class InventoryReplica {
public:
    InventoryReplica(std::uint32_t account_id,std::uint32_t money,InventoryReplicaPool& pool):account_id_(account_id),money_(money),pool_(pool) {}
    ~InventoryReplica();
    InventoryReplica(const InventoryReplica&)=delete;InventoryReplica& operator=(const InventoryReplica&)=delete;
    bool load(PlayerItem,bool bank=false,std::int32_t save_type=0);
    // LoadCharacter duplicate: only a zero pooled save marker becomes 2.
    bool load_relocated(PlayerItem,bool bank=false);
    // Attach an already allocated row without changing its pooled markers.
    void adopt_loaded(std::unique_ptr<CachedInventoryItem>,bool bank=false);
    bool apply_money(std::span<const std::uint8_t>); // Shared owned 0603 status field.
    InventoryReplicaResult apply(std::span<const std::uint8_t>);
    std::vector<CachedInventoryItem> inventory() const;
    std::vector<CachedInventoryItem> bank() const;
    const std::vector<std::uint64_t>& deleted_inventory() const noexcept {return deleted_inventory_;}
    const std::vector<std::uint64_t>& deleted_bank() const noexcept {return deleted_bank_;}
    std::uint32_t money() const noexcept {return money_;}
    std::uint32_t account_id() const noexcept {return account_id_;}
    // Inventory/bank portion of CUser::SaveSuccess (00409560). Caller must hold
    // exclusive cache ownership through the complete character transaction and
    // call this only after its successful commit. Rollback leaves this untouched.
    void inventory_save_success() noexcept;
private:
    friend class PointItemReplica;
    friend class PurchaseReplica;
    using Item=std::unique_ptr<CachedInventoryItem>;
    bool load_impl(PlayerItem,bool,std::int32_t,bool);
    void journal(const Item&,bool bank);
    std::uint32_t account_id_,money_;InventoryReplicaPool& pool_;
    std::array<Item,144> inventory_{};std::array<Item,240> bank_{};
    std::vector<std::uint64_t> deleted_inventory_,deleted_bank_;
};
}
