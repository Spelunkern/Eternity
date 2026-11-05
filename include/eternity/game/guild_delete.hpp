#pragma once
#include "eternity/game/guild_save.hpp"
#include "eternity/game/inventory_replica.hpp"
#include <cstdint>

namespace eternity::game {
// ps_dbAgent.exe PacketGuild 0040ba00 case 0b05; DBGuild::DeleteGuild
// 004282a0 retires the retained GuildMgrSave replica only after SQL success.
inline InventoryReplicaResult guild_delete(std::span<const std::uint8_t> p,
    const std::function<GuildSaveResult(std::uint32_t)>& execute,
    const std::function<void(const GuildSaveResult&)>& diagnostic,
    const std::function<void(std::uint32_t)>& retire) {
    if(p.size()<2)return InventoryReplicaResult::rejected;
    if(p[0]!=5 || p[1]!=11)return InventoryReplicaResult::unsupported;
    if(p.size()!=6)return InventoryReplicaResult::rejected;
    if(!execute || !diagnostic || !retire)throw std::invalid_argument("Missing guild delete service");
    std::uint32_t guild=0;for(unsigned i=0;i<4;++i)guild|=std::uint32_t{p[2+i]}<<(8*i);
    auto apply=[&](const GuildSaveResult& result){if(!result.success)return false;retire(guild);return true;};
    auto result=execute(guild);if(apply(result))return InventoryReplicaResult::applied;
    const bool timed_out=result.sqlstate=="HYT00";if(!timed_out)diagnostic(result);
    result=execute(guild);if(apply(result))return InventoryReplicaResult::applied;
    if(timed_out || result.sqlstate!="HYT00")diagnostic(result);
    if(timed_out){result=execute(guild);if(apply(result))return InventoryReplicaResult::applied;diagnostic(result);}
    throw std::runtime_error("Guild delete failed");
}
}
