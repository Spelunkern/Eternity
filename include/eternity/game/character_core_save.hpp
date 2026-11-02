#pragma once
#include "eternity/game/character_replica.hpp"

namespace eternity::game {
struct CharacterCoreSaveOperation {std::string procedure;std::vector<storage::Parameter> parameters;};
// First two phases of DBCharacter::SaveCharacter 00423190: Info, optional BodyChg.
std::vector<CharacterCoreSaveOperation> character_core_save_plan(const CharacterReplica&);
class CharacterCoreStore {
public:
    explicit CharacterCoreStore(storage::SqlServer&,std::string catalog="PS_GameData");
    // Caller owns the complete character transaction and exclusive replica access.
    // No commit, rollback or acknowledgment is performed here.
    void save_in_transaction(const CharacterReplica&);
private:
    storage::SqlServer& database_;std::string prefix_;
};
}
