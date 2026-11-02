#pragma once
#include "eternity/game/inventory_replica.hpp"

namespace eternity::game {
struct InventorySaveOperation {
    std::string procedure;
    std::vector<storage::Parameter> parameters;
    // Failed inserts delete this UID globally (ChkID=0), then retry once.
    std::string retry_delete_procedure;
    std::vector<storage::Parameter> retry_delete_parameters;
};
// DBCharacter::SaveCharacter 00423190, inventory/bank phase only. Deletions
// precede slot-ordered dirty rows; this does not replace other character phases.
std::vector<InventorySaveOperation> inventory_save_plan(const InventoryReplica&,std::uint8_t server_id,std::uint32_t character_id);
class InventoryStore {
public:
    InventoryStore(storage::SqlServer&,std::uint8_t server_id,std::string catalog="PS_GameData");
    // Requires a caller-owned transaction and exclusively owned cache until
    // commit + inventory_save_success(). Never commits, rolls back or acknowledges.
    void save_in_transaction(const InventoryReplica&,std::uint32_t character_id);
private:
    storage::SqlServer& database_;std::uint8_t server_id_;std::string prefix_;
};
}
