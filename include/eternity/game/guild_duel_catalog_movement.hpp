#pragma once
#include "eternity/game/guild_duel_catalog_world.hpp"

namespace eternity::game {
// Binds CGuildLock to the current actor's actual GuildUser and catalog-owned
// guild. The world executor serializes membership changes; all operations that
// touch a LoadedGuild use its same recursive mutex. GuildUser/session objects
// and catalog survive the lock token and callbacks.
class GuildDuelCatalogMovement:public GuildDuelMovementAdapter {
public:
    GuildDuelCatalogMovement(GuildCatalog& catalog,GuildDuelCatalogWorld& world)
        :GuildDuelMovementAdapter(world),catalog_(catalog),world_(world){}
    std::shared_ptr<void> capture_guild_lock(Actor& actor) final {
        auto& user=guild_user(actor);validate(actor,user);auto* captured=catalog_.find_membership(user.guild);
        if(!captured) {if(user.guild)throw std::logic_error("Guild membership is not in catalog");return {};}
        auto lock=world_.lock(captured->duel);
        // Original rechecks only whether the current pointer became null; it
        // still unlocks the originally captured object when membership changes.
        if(!user.guild)return {};
        return lock;
    }
    void* current_guild(Actor& actor) final {
        auto& user=guild_user(actor);validate(actor,user);auto* guild=catalog_.find_membership(user.guild);
        if(!guild && user.guild)throw std::logic_error("Guild membership is not in catalog");
        return guild;
    }
    GuildDuelState& duel_state(void* guild) final {
        if(!guild)throw std::invalid_argument("Missing live guild");
        auto* loaded=static_cast<LoadedGuild*>(guild);
        if(catalog_.find_membership(&loaded->membership)!=loaded)throw std::logic_error("Foreign guild owner");
        return loaded->duel;
    }
    virtual GuildUser& guild_user(Actor&)=0;
private:
    static void validate(const Actor& actor,const GuildUser& user) {
        if(actor.id()!=user.character_id)throw std::logic_error("Guild user belongs to another actor");
    }
    GuildCatalog& catalog_;GuildDuelCatalogWorld& world_;
};
}
