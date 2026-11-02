#pragma once
#include "eternity/game/quest_replica.hpp"

namespace eternity::game {
struct QuestSaveOperation {std::string procedure;std::vector<storage::Parameter> parameters;};
// DBCharacter::SaveCharacter 004242c2..00424617: deleted, active, then finished.
std::vector<QuestSaveOperation> quest_save_plan(const QuestReplica&,std::uint32_t character_id);
class QuestStore {
public:
    explicit QuestStore(storage::SqlServer&,std::string catalog="PS_GameData");
    // Requires the complete caller-owned character transaction and exclusive cache
    // access. Only after commit may QuestReplica::save_success acknowledge flags.
    void save_in_transaction(const QuestReplica&,std::uint32_t character_id);
private:
    storage::SqlServer& database_;std::string prefix_;
};
}
