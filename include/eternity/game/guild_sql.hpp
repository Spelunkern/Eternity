#pragma once
#include "eternity/game/guild_catalog.hpp"
#include "eternity/game/guild_bank_save.hpp"
#include "eternity/game/guild_npc_save.hpp"
#include "eternity/game/guild_save.hpp"
#include "eternity/storage/sql_server.hpp"

namespace eternity::game {
// DB-agent metadata replica. Existing rows retain their values on another load.
struct GuildDbMetadata {
    GuildMetadata metadata;GuildNpcReplica npcs;std::unique_ptr<GuildBankReplica> bank;std::int32_t save_type=0;
    void set_remark(std::string_view); // UpdateGuildRemark 0040fdf0.
    bool upgrade_npc(std::uint8_t slot,std::uint16_t type,std::uint8_t level,std::uint32_t etin,std::uint32_t keep);
    void reset_npcs(std::uint32_t keep);
};
struct GuildSqlPorts {
    std::function<void(net::Packet)> send;
    std::function<bool()> allocate;
    std::function<void(const std::string&)> diagnostic;
};
// One DB executor owns the loader; callbacks must not reenter it.
class GuildSql {
public:
    GuildSql(storage::SqlServer&,storage::SqlServer&,InventoryReplicaPool&,GuildSqlPorts,std::string catalog="PS_GameData");
    bool load_maximum(); // LoadMaxID 00426a00.
    bool load(); // LoadGuildNUserList 004271a0 / LoadUserList 004275d0.
    bool save(); // DBSave 00410190: metadata, bank/NPC passes and original retries.
    void send_npcs(); // SendGuildNpcLvList 00410cb0, after guild/bank loading.
    InventoryReplicaResult apply(std::span<const std::uint8_t>); // Owned bank/NPC commands, PacketGuild 0040ba00.
    std::optional<std::uint32_t> save_bank(std::uint32_t guild); // One DBSaveItem pass; global scheduling is separate.
    std::optional<std::uint32_t> save_npcs(std::uint32_t guild); // One DBSaveNpcLv pass.
    void send_banks(); // SendGuildBankList 00410c20.
    const std::map<std::uint32_t,GuildDbMetadata>& replicas() const noexcept {return replicas_;}
private:
    bool members(std::uint32_t);
    bool load_npcs(std::uint32_t,GuildNpcReplica&);
    bool load_bank(std::uint32_t,GuildBankReplica&);
    storage::SqlServer& main_;storage::SqlServer& auxiliary_;InventoryReplicaPool& pool_;GuildBankStore bank_store_;GuildNpcStore npc_store_;GuildSqlPorts ports_;std::string prefix_;
    std::map<std::uint32_t,GuildDbMetadata> replicas_;
};
}
