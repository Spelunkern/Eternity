#pragma once
#include "eternity/game/inventory_replica.hpp"

namespace eternity::game {
struct CachedProductItem {
    std::int32_t save_type=0;
    std::uint8_t type=0,type_id=0,count=0;
    PlayerItem receipt;
};
struct CachedPointItem {std::int32_t save_type=0;PlayerItem receipt;};
class PointItemPool {
public:
    using Allocator=std::function<std::unique_ptr<CachedPointItem>()>;
    explicit PointItemPool(Allocator allocator={}):allocator_(std::move(allocator)) {}
    std::unique_ptr<CachedPointItem> allocate();
    void release(std::unique_ptr<CachedPointItem>);
    InventoryReplicaPoolStatistics statistics() const;
private:
    mutable std::mutex mutex_;Allocator allocator_;std::uint32_t attempts_=0;
    std::vector<std::unique_ptr<CachedPointItem>> available_;
};
// Account-owned products and stored point items share the character inventory.
// Both pools and the inventory must outlive this serialized cache.
class PointItemReplica {
public:
    PointItemReplica(InventoryReplica& inventory,PointItemPool& pool):inventory_(inventory),pool_(pool) {}
    ~PointItemReplica();
    void load_products(std::span<const PlayerPointItem>);
    void load_stored(std::span<const PlayerPointItem>);
    bool load(const PlayerSnapshot&);
    InventoryReplicaResult apply(std::span<const std::uint8_t>); // 0710 / 0e05
    const std::array<CachedProductItem,240>& products() const noexcept {return products_;}
    std::vector<std::pair<std::uint8_t,CachedPointItem>> stored() const;
    std::optional<net::Packet> product_packet() const; // 040c, omit empty count lists
    std::optional<net::Packet> stored_packet() const; // 0e04, includes pending receipts
    std::uint32_t account_id() const noexcept {return inventory_.account_id();}
    // Full SaveSuccess clears product flags and recycles state-3 stored items.
    void point_save_success();
private:
    InventoryReplica& inventory_;PointItemPool& pool_;
    std::array<CachedProductItem,240> products_{};
    std::array<std::unique_ptr<CachedPointItem>,240> stored_{};
};
struct PointSaveOperation {std::string procedure;std::vector<storage::Parameter> parameters;};
std::vector<PointSaveOperation> point_save_plan(const PointItemReplica&,std::uint8_t server,std::uint32_t character);
void execute_point_save(std::span<const PointSaveOperation>,const std::function<std::int64_t(const PointSaveOperation&)>&);
class PointItemStore {
public:
    PointItemStore(storage::SqlServer&,std::uint8_t server,std::string catalog="PS_GameData");
    void load_products(PointItemReplica&);
    // After core/body, before inventory writes. Fatal on SQL/API or negative return.
    // Caller owns the complete transaction and acknowledges only after its commit.
    void save_in_transaction(const PointItemReplica&,std::uint32_t character);
private:
    storage::SqlServer& database_;std::uint8_t server_;std::string prefix_;
};
}
