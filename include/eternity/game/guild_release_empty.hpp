#pragma once
#include "eternity/game/guild_catalog.hpp"
#include "eternity/game/actor.hpp"

namespace eternity::game {
// CGuild::Release 0042d5a0, restricted to the empty-membership case reached
// after the last user leaves. Caller owns the world executor and retains the
// captured guild, acting actor and log service through reentrant callbacks.
inline void guild_release_empty(LoadedGuild& guild,GuildCatalog& catalog,Actor* actor,
    const std::function<void(Actor*,const LoadedGuild&)>& game_log) {
    if(catalog.find_membership(&guild.membership)!=&guild)throw std::logic_error("Foreign guild release owner");
    if(!game_log)throw std::invalid_argument("Missing guild release log service");
    std::lock_guard guard(guild.mutex);auto& membership=guild.membership;
    auto require_empty=[&]{if(!membership.online.empty() || !membership.offline.empty())throw std::logic_error("Guild release requires empty membership");};
    require_empty();membership.broadcast({12,13,0x65,0,0,0,0});require_empty();
    game_log(actor,guild);require_empty();
    while(!membership.applications.empty()) {
        auto row=membership.applications.begin();if(!row->second)break;
        auto node=membership.applications.extract(row);catalog.directory().remove(node.mapped()->id);
    }
    guild.metadata.officers=0;for(auto& leader:membership.leaders)leader.reset();
}
}
