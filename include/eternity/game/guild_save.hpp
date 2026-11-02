#pragma once
#include "eternity/storage/sql_server.hpp"
#include <functional>

namespace eternity::game {
struct GuildDbMetadata;
struct GuildSaveOperation {std::string procedure;std::array<storage::Parameter,9> parameters;};
// Original sprintf layout, including raw remark quotes and C-string termination.
std::string guild_save_sql(const GuildSaveOperation&,std::string_view catalog_prefix={});
struct GuildSaveResult {bool success=true;std::string sqlstate;std::int32_t native_error=0;};
struct GuildSavePorts {
    std::function<bool()> acquire;
    std::function<void()> release,prepare;
    std::function<GuildSaveResult(const GuildSaveOperation&)> execute;
    std::function<std::int32_t(GuildDbMetadata&)> bank,npcs;
    std::function<void(const GuildSaveResult&)> diagnostic;
};
// GuildMgrSave::DBSave, ps_dbAgent.exe 00410190. Caller owns all records in
// unsigned guild-ID order throughout the call. False means lease/third timeout
// failure; true does not acknowledge dirty child state that still failed retry.
bool save_guilds(std::span<GuildDbMetadata* const> ordered,GuildSavePorts&);
}
