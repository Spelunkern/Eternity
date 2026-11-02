#pragma once
#include "eternity/game/inventory_replica.hpp"
#include "eternity/storage/sql_server.hpp"

namespace eternity::game {
class GuildBankReplica;
struct GuildBankSaveOperation {std::string procedure;std::vector<storage::Parameter> parameters;};
enum class GuildBankSqlResult {success,bind_error,query_error,results_error,negative_result,lease_error};
struct GuildBankSavePorts {
    std::function<bool()> acquire;
    std::function<void()> release,prepare;
    std::function<GuildBankSqlResult(const GuildBankSaveOperation&)> execute;
    std::function<void(GuildBankSqlResult)> diagnostic;
};
// GuildMgrSave::DBSaveItem, ps_dbAgent.exe 004105b0. Caller exclusively owns
// all slots and journal throughout the call. Each successful operation commits
// independently; this is not a character transaction or a deferred acknowledgment.
// Ports must not reenter the owner. Returns the original error count.
std::uint32_t save_guild_bank(std::uint32_t guild,std::span<CachedInventoryItem* const,240> slots,
    std::vector<std::uint64_t>& deleted,GuildBankSavePorts&);
class GuildBankStore {
public:
    GuildBankStore(storage::SqlServer&,std::function<void(const std::string&)> diagnostic,std::string catalog="PS_GameData");
    std::uint32_t save(std::uint32_t guild,GuildBankReplica&);
private:
    storage::SqlServer& database_;std::string prefix_;std::function<void(const std::string&)> diagnostic_;
};
}
