#pragma once
#include "eternity/game/guild_save.hpp"
#include "eternity/game/inventory_replica.hpp"
#include <cstdint>

namespace eternity::game {
// ps_dbAgent.exe PacketGuild 0040ba00 case 0b08; DBGuild::DeleteChatacter 00428560.
inline InventoryReplicaResult guild_member_delete(std::span<const std::uint8_t> p,
    const std::function<GuildSaveResult(std::uint32_t,std::uint32_t)>& execute,
    const std::function<void(const GuildSaveResult&)>& diagnostic) {
    if(p.size()<2)return InventoryReplicaResult::rejected;
    if(p[0]!=8 || p[1]!=11)return InventoryReplicaResult::unsupported;
    if(p.size()!=10)return InventoryReplicaResult::rejected;
    if(!execute || !diagnostic)throw std::invalid_argument("Missing guild member delete SQL service");
    auto dword=[&](std::size_t at){std::uint32_t v=0;for(unsigned i=0;i<4;++i)v|=std::uint32_t{p[at+i]}<<(8*i);return v;};
    const auto guild=dword(2),character=dword(6);
    auto result=execute(guild,character);if(result.success)return InventoryReplicaResult::applied;
    const bool timed_out=result.sqlstate=="HYT00";if(!timed_out)diagnostic(result);
    result=execute(guild,character);if(result.success)return InventoryReplicaResult::applied;
    if(timed_out || result.sqlstate!="HYT00")diagnostic(result);
    if(timed_out) {result=execute(guild,character);if(result.success)return InventoryReplicaResult::applied;diagnostic(result);}
    throw std::runtime_error("Guild member delete failed");
}
}
