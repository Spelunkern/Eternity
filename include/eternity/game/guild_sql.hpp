#pragma once
#include "eternity/game/guild_catalog.hpp"
#include "eternity/storage/sql_server.hpp"

namespace eternity::game {
// DB-agent metadata replica. Existing rows retain their values on another load.
struct GuildDbMetadata {GuildMetadata metadata;GuildNpcReplica npcs;};
struct GuildSqlPorts {
    std::function<void(net::Packet)> send;
    std::function<bool()> allocate;
    // AddFromDB invokes this only for a new replica, ignoring its result.
    // The bank owner remains required; NPC SQL loading runs natively afterward.
    std::function<void(std::uint32_t)> load_bank;
    std::function<void(const std::string&)> diagnostic;
};
// One DB executor owns the loader; callbacks must not reenter it.
class GuildSql {
public:
    GuildSql(storage::SqlServer&,storage::SqlServer&,GuildSqlPorts,std::string catalog="PS_GameData");
    bool load_maximum(); // LoadMaxID 00426a00.
    bool load(); // LoadGuildNUserList 004271a0 / LoadUserList 004275d0.
    void send_npcs(); // SendGuildNpcLvList 00410cb0, after guild/bank loading.
    const std::map<std::uint32_t,GuildDbMetadata>& replicas() const noexcept {return replicas_;}
private:
    bool members(std::uint32_t);
    bool load_npcs(std::uint32_t,GuildNpcReplica&);
    storage::SqlServer& main_;storage::SqlServer& auxiliary_;GuildSqlPorts ports_;std::string prefix_;
    std::map<std::uint32_t,GuildDbMetadata> replicas_;
};
}
