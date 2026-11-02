#pragma once
#include "eternity/game/point_items.hpp"

namespace eternity::game {
struct PurchaseLog {
    std::int32_t save_type=0;
    std::array<std::uint8_t,21> code{};
    std::uint32_t made_at=0,points=0;
};
struct PendingPurchase {std::uint8_t log_slot=0;std::vector<CachedInventoryItem> items;};
enum class PurchaseAction {save,queue_exit,exit}; // Original DB jobs 18 / 9 / ExitUser.
struct PurchaseSavePort;
struct PurchaseSaveResult;
class PurchaseReplica {
public:
    explicit PurchaseReplica(InventoryReplica& inventory):inventory_(inventory) {}
    ~PurchaseReplica();
    bool load(const PlayerSnapshot&);
    void load_history(std::span<const PurchaseLog>,std::uint32_t cash_points);
    InventoryReplicaResult apply(std::span<const std::uint8_t>);
    void set_in_game(bool value) noexcept {in_game_=value;}
    const std::array<PurchaseLog,10>& history() const noexcept {return history_;}
    std::vector<PendingPurchase> pending() const;
    std::size_t pending_count() const noexcept {return pending_.size();}
    std::span<const PurchaseAction> actions() const noexcept {return actions_;}
    void clear_actions() noexcept {actions_.clear();}
    net::Packet packet() const; // 0e01, including an empty history.
    std::uint32_t account_id() const noexcept {return inventory_.account_id();}
private:
    friend PurchaseSaveResult execute_purchase_save(PurchaseReplica&,std::uint32_t,const PurchaseSavePort&);
    struct OwnedPurchase {std::uint8_t log_slot=0;std::vector<std::unique_ptr<CachedInventoryItem>> items;};
    void release(OwnedPurchase&);
    InventoryReplica& inventory_;std::array<PurchaseLog,10> history_{};
    std::vector<OwnedPurchase> pending_;std::vector<PurchaseAction> actions_;
    std::uint32_t cash_points_=0;bool in_game_=true;
};
enum class PurchaseConnection {items,points};
// These callbacks model original pool/ODBC ports. Transaction callbacks return
// success; query may throw runtime_error. release/timeout/diagnostic must not throw.
struct PurchaseSavePort {
    std::function<bool(PurchaseConnection)> acquire,begin,commit,rollback,timeout;
    std::function<void(PurchaseConnection)> release;
    std::function<std::int64_t(PurchaseConnection,const PointSaveOperation&)> query;
    std::function<void(std::string_view)> diagnostic;
};
struct PurchaseSaveResult {bool legacy_success=true;std::size_t saved=0,retained=0,discarded=0;};
std::vector<PointSaveOperation> purchase_save_plan(const PurchaseReplica&,const PendingPurchase&,std::uint32_t character);
PurchaseSaveResult execute_purchase_save(PurchaseReplica&,std::uint32_t character,const PurchaseSavePort&);
// Concrete two-connection adapter. Point charges commit independently of item
// writes, matching the original; callers must not enclose it in character saving.
class PurchaseStore {
public:
    PurchaseStore(storage::SqlServer& items,storage::SqlServer& points,std::string catalog="PS_GameData");
    PurchaseSaveResult save(PurchaseReplica&,std::uint32_t character,const std::function<void(std::string_view)>& diagnostic={});
private:
    storage::SqlServer& items_;storage::SqlServer& points_;std::string prefix_;
};
}
