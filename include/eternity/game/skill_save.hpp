#pragma once
#include "eternity/game/skill_replica.hpp"

namespace eternity::game {
struct SkillSaveOperation {std::string procedure;std::vector<storage::Parameter> parameters;};
// Learned and applied skill phase of DBCharacter::SaveCharacter 00423f55..004242c2.
std::vector<SkillSaveOperation> skill_save_plan(const SkillReplica&,std::uint32_t character_id);
class SkillStore {
public:
    explicit SkillStore(storage::SqlServer&,std::string catalog="PS_GameData");
    // Caller owns the complete character transaction and exclusive replica access.
    // Only after its commit may SkillReplica::save_success acknowledge the cache.
    void save_in_transaction(const SkillReplica&,std::uint32_t character_id);
private:
    storage::SqlServer& database_;std::string prefix_;
};
}
