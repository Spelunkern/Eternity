#pragma once
#include "eternity/game/inventory_replica.hpp"

namespace eternity::game {
struct AccountSaveOperation {std::string procedure;std::vector<storage::Parameter> parameters;};
class MarketReplica {
public:
    explicit MarketReplica(std::uint32_t account):account_(account) {}
    bool load(std::span<const std::uint32_t>);
    bool load(const PlayerSnapshot&);
    InventoryReplicaResult apply(std::span<const std::uint8_t>);
    std::span<const std::uint32_t> ids() const noexcept {return {ids_.data(),count_};}
    std::int32_t save_type() const noexcept {return save_type_;}
    std::optional<net::Packet> packet() const; // 0510; original emits nothing when empty.
    // Source clears this before the surrounding transaction commits, not in SaveSuccess.
    void sql_saved() noexcept {save_type_=0;}
private:
    std::uint32_t account_;std::array<std::uint32_t,10> ids_{};std::size_t count_=0;std::int32_t save_type_=0;
};
std::vector<AccountSaveOperation> market_save_plan(const MarketReplica&,std::uint32_t character_id);
// Diagnostic callbacks run immediately and must not throw.
struct MarketSaveResult {std::optional<std::string> failure;};
MarketSaveResult execute_market_save(MarketReplica&,std::uint32_t character_id,const std::function<void(const AccountSaveOperation&)>&,const std::function<void(std::string_view)>& diagnostic={});
class MarketStore {
public:
    explicit MarketStore(storage::SqlServer&,std::string catalog="PS_GameData");
    // Last phase of the caller-owned character transaction. SQL errors log and
    // leave market pending, but do not throw; partial list writes can commit.
    MarketSaveResult save_in_transaction(MarketReplica&,std::uint32_t character_id,const std::function<void(std::string_view)>& diagnostic={});
private:
    storage::SqlServer& database_;std::string prefix_;
};
class GrowthReplica {
public:
    explicit GrowthReplica(std::uint8_t maximum=2):maximum_(maximum) {}
    void load(std::uint8_t maximum) noexcept {maximum_=maximum;}
    std::uint8_t maximum() const noexcept {return maximum_;}
    bool advance(std::uint8_t grow,std::uint16_t level) noexcept;
private:
    std::uint8_t maximum_;
};
struct GrowthSaveResult {std::int32_t legacy_code=0;bool advanced=false;std::optional<std::string> failure;};
// Null user -> code 1; unavailable connection -> 3; negative procedure return -> -1.
// Original API errors log but return code 0. A failed write never restores maximum.
// Executor nullopt models the original DB-pool allocation failure.
GrowthSaveResult execute_growth_save(GrowthReplica*,std::uint8_t server,std::uint32_t account,std::uint8_t grow,std::uint16_t level,const std::function<std::optional<std::int64_t>(const AccountSaveOperation&)>&,const std::function<void(std::string_view)>& diagnostic={});
class GrowthStore {
public:
    GrowthStore(storage::SqlServer&,std::uint8_t server,std::string catalog="PS_GameData");
    // Original SaveMaxGrow precedes the complete character transaction, using its
    // own DB allocation. Use a connection outside that transaction.
    GrowthSaveResult save(GrowthReplica&,std::uint32_t account,std::uint8_t grow,std::uint16_t level,const std::function<void(std::string_view)>& diagnostic={});
private:
    storage::SqlServer& database_;std::uint8_t server_;std::string prefix_;
};
}
