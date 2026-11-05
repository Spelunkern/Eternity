#pragma once
#include "eternity/game/guild_save.hpp"
#include "eternity/game/inventory_replica.hpp"
#include <cstdint>

namespace eternity::game {
// ps_dbAgent.exe CConnection::PacketGuild 0040ba00, 0b09 at 0040bbec;
// DBGuild::UpdateChatacter 004286c0. The guild ID is unused by the procedure.
inline InventoryReplicaResult guild_member_update(std::span<const std::uint8_t> p,
    const std::function<GuildSaveResult(std::uint32_t,std::uint8_t)>& execute,
    const std::function<void(const GuildSaveResult&)>& diagnostic) {
    if(p.size()<2)return InventoryReplicaResult::rejected;
    if(p[0]!=9 || p[1]!=11)return InventoryReplicaResult::unsupported;
    if(p.size()!=11)return InventoryReplicaResult::rejected;
    if(!execute || !diagnostic)throw std::invalid_argument("Missing guild member update SQL service");
    std::uint32_t character=0;for(unsigned i=0;i<4;++i)character|=std::uint32_t{p[6+i]}<<(8*i);
    auto result=execute(character,p[10]);if(result.success)return InventoryReplicaResult::applied;
    const bool timed_out=result.sqlstate=="HYT00";
    if(!timed_out)diagnostic(result);
    result=execute(character,p[10]);if(result.success)return InventoryReplicaResult::applied;
    if(timed_out || result.sqlstate!="HYT00")diagnostic(result);
    if(timed_out) {
        result=execute(character,p[10]);if(result.success)return InventoryReplicaResult::applied;
        diagnostic(result);
    }
    // The original emits no success response. Native callers must observe a
    // failed write rather than treating an exhausted retry as persisted.
    throw std::runtime_error("Guild member rank update failed");
}
}
