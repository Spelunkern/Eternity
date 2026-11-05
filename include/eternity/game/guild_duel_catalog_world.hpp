#pragma once
#include "eternity/game/guild_catalog.hpp"
#include "eternity/game/guild_duel_membership_world.hpp"
namespace eternity::game {
// Binds duel lookup and membership directly to the retained GuildCatalog; no
// parallel guild state/index is created. Catalog and its loaded guilds outlive
// this world. The owner supplies real guild locks and recipient connections.
class GuildDuelCatalogWorld:public GuildDuelMembershipWorld {
public:
    GuildDuelCatalogWorld(GuildCatalog& catalog,GuildDuelBroadcastServices& services)
        :GuildDuelMembershipWorld(services),catalog_(catalog){}
    GuildDuelState* find_guild(std::uint32_t id) final {
        auto* guild=catalog_.find(id);return guild?&guild->duel:nullptr;
    }
    std::shared_ptr<void> lock(GuildDuelState& state) final {
        auto& guild=owner(state);
        return std::make_shared<std::unique_lock<std::recursive_mutex>>(guild.mutex);
    }
    GuildMembership& membership(GuildDuelState& state) final {return owner(state).membership;}
private:
    LoadedGuild& owner(GuildDuelState& state) const {
        auto* guild=static_cast<LoadedGuild*>(state.guild);
        if(!guild || &guild->duel!=&state || catalog_.find(guild->metadata.listing.id)!=guild)
            throw std::logic_error("Guild duel state is not owned by this catalog");
        return *guild;
    }
    GuildCatalog& catalog_;
};
}
