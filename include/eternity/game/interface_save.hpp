#pragma once
#include "eternity/game/interface_replica.hpp"

namespace eternity::game {
struct InterfaceSaveOperation {std::string procedure;std::vector<storage::Parameter> parameters;bool quick=false;};
struct InterfaceSaveResult {std::optional<std::string> quick_failure;};
std::vector<InterfaceSaveOperation> interface_save_plan(const InterfaceReplica&,std::uint32_t character_id,bool save_quick_slots);
// Original quick-slot errors stop that phase and log, then point saving continues.
// Point errors propagate to the complete character rollback owner.
// Diagnostic callback runs immediately on a quick error and must not throw.
InterfaceSaveResult execute_interface_save_plan(std::span<const InterfaceSaveOperation>,const std::function<void(const InterfaceSaveOperation&)>&,const std::function<void(std::string_view)>& diagnostic={});
class InterfaceStore {
public:
    explicit InterfaceStore(storage::SqlServer&,std::string catalog="PS_GameData");
    // Caller owns the complete character transaction and exclusively owns the cache.
    // The original periodic caller passes false for save_quick_slots.
    InterfaceSaveResult save_in_transaction(const InterfaceReplica&,std::uint32_t character_id,bool save_quick_slots,const std::function<void(std::string_view)>& diagnostic={});
private:
    storage::SqlServer& database_;std::string prefix_;
};
}
