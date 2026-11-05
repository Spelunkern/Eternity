#pragma once
#include "eternity/game/guild_membership.hpp"
#include "eternity/game/vertical_move_guild_duel.hpp"

namespace eternity::game {
class GuildDuelResetServices {
public:
    virtual ~GuildDuelResetServices()=default;
    // Resolve and pin the exact live Actor corresponding to this GuildUser;
    // a nonempty token keeps its scene and guild-duel state alive.
    virtual Actor& actor(GuildUser&)=0;
    virtual std::shared_ptr<void> retain(Actor&)=0;
    virtual VerticalGuildDuelMovement& movement_state(Actor&)=0;
};
// CGuild::PvPResetUsers 00438070. Caller holds the actual membership lock;
// membership maps and record lifetimes cannot structurally change in callbacks.
// Null source records terminate traversal, matching the original tree walk.
inline void guild_duel_reset_members(GuildMembership& guild,GuildDuelResetServices& services) {
    for(const auto& [id,record]:guild.online) {
        (void)id;if(!record)break;if(!record->user)continue;
        auto& actor=services.actor(*record->user);if(actor.id()!=record->user->character_id)
            throw std::logic_error("Guild member resolved to another actor");
        auto pin=services.retain(actor);if(!pin)throw std::logic_error("Cannot retain guild duel member");
        actor.clear_duel_debuffs();actor.reset_duel_damage();actor.relations().opposing_guild_id=0;
        auto& state=services.movement_state(actor);state.state=-1;actor.status().current_target_type=0;
        actor.relations().guild_duel_ready=0;state.ready_flag=0;
    }
}
}

