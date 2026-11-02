#pragma once
#include "eternity/game/guild_npcs.hpp"
#include "eternity/storage/sql_server.hpp"
#include <functional>

namespace eternity::game {
struct GuildNpcSaveOperation {std::string procedure;std::array<std::int64_t,4> parameters;};
struct GuildNpcSavePorts {
    std::function<bool()> acquire;
    std::function<void()> release,prepare;
    std::function<bool(const GuildNpcSaveOperation&)> execute;
    std::function<void()> diagnostic;
};
// DBSaveNpcLv 004103e0: exclusive cache ownership through each pass.
// Query failures retain their markers and do not stop other slots. Procedure
// return values are not bound/read. Returns the original error count.
std::uint32_t save_guild_npcs(std::uint32_t guild,GuildNpcReplica&,GuildNpcSavePorts&);
class GuildNpcStore {
public:
    GuildNpcStore(storage::SqlServer&,std::function<void(const std::string&)>,std::string catalog="PS_GameData");
    std::uint32_t save(std::uint32_t guild,GuildNpcReplica&);
private:
    storage::SqlServer& database_;std::function<void(const std::string&)> diagnostic_;std::string prefix_;
};
}
