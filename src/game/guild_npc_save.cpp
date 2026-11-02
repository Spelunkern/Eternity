#include "eternity/game/guild_npc_save.hpp"
#include <bit>

namespace eternity::game {
std::uint32_t save_guild_npcs(std::uint32_t guild,GuildNpcReplica& npcs,GuildNpcSavePorts& ports) {
    if(!ports.acquire || !ports.release || !ports.prepare || !ports.execute || !ports.diagnostic) throw std::invalid_argument("Incomplete guild NPC persistence ports");
    bool acquired=false;std::uint32_t errors=0;
    struct Lease {GuildNpcSavePorts& ports;bool& active;~Lease(){if(active) ports.release();}} lease{ports,acquired};
    for(std::size_t i=0;i<npcs.slots.size();++i) {
        auto& npc=npcs.slots[i];if(npc.type==0 || (npc.marker!=1 && npc.marker!=2)) continue;
        if(acquired) ports.prepare();else {acquired=ports.acquire();if(!acquired) {ports.diagnostic();return 1;}}
        const GuildNpcSaveOperation operation{npc.marker==1?"usp_save_guild_npclv_add_e":"usp_save_guild_npclv_mod_e",{std::bit_cast<std::int32_t>(guild),npc.type,npc.level,static_cast<std::int64_t>(i)}};
        if(ports.execute(operation)) npc.marker=0;else {++errors;ports.diagnostic();}
    }
    return errors;
}
GuildNpcStore::GuildNpcStore(storage::SqlServer& database,std::function<void(const std::string&)> diagnostic,std::string catalog):database_(database),diagnostic_(std::move(diagnostic)) {
    if(!diagnostic_ || catalog.empty() || catalog.size()>128 || catalog.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_")!=std::string::npos) throw std::invalid_argument("Invalid guild NPC SQL services/catalog");prefix_="["+catalog+"].dbo.";
}
std::uint32_t GuildNpcStore::save(std::uint32_t guild,GuildNpcReplica& npcs) {
    if(database_.transaction_active()) throw std::logic_error("Guild NPC saving requires independent autocommit operations");std::string diagnostic;
    GuildNpcSavePorts ports{[]{return true;},[]{},[]{},[&](const GuildNpcSaveOperation& operation) {
        std::array<storage::Parameter,4> parameters;for(std::size_t i=0;i<4;++i) parameters[i]=operation.parameters[i];
        try {(void)database_.query("EXEC "+prefix_+operation.procedure+" ?,?,?,?;",parameters);return true;}
        catch(const std::exception& error) {diagnostic=error.what();return false;}
    },[&]{diagnostic_(diagnostic);}};
    return save_guild_npcs(guild,npcs,ports);
}
}
