#pragma once
#include "eternity/game/character_replica.hpp"
#include <filesystem>
#include <mutex>

namespace eternity::game {
struct RecoveryOperation {std::string procedure;std::vector<storage::Parameter> parameters;bool odbc=true;};
// DBCharacter::SaveToFile 00424ce0. Caller exclusively owns the replica. Capturing
// recovery SQL mutates original save markers even when later file I/O fails.
// Optional output follows original statement order; it must not throw or reenter
// the replica. The file adapter flushes each line before continuing.
std::vector<RecoveryOperation> capture_character_recovery(CharacterReplica&,std::uint8_t server,const std::function<void(const RecoveryOperation&)>& output={});
std::string recovery_sql(const RecoveryOperation&);
struct RecoveryWriteResult {bool legacy_success=true,persisted=false;std::size_t operations=0;};
class CharacterRecoveryFile {
public:
    explicit CharacterRecoveryFile(std::filesystem::path,std::function<std::string()> clock={});
    RecoveryWriteResult append(CharacterReplica&,std::uint8_t server,std::string_view username);
private:
    std::filesystem::path path_;std::function<std::string()> clock_;std::mutex mutex_;
};
}
