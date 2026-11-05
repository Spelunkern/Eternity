#pragma once
#include "eternity/game/vertical_move_pvp_interactions.hpp"
#include "eternity/game/vertical_session_registry.hpp"
#include "eternity/game/zone_users.hpp"

namespace eternity::game {
// The world strand owns the zone, sessions, duel state and PvP services. The
// guild callback must perform the real locked guild-mark refresh. NPC lifetime
// and all registry/zone mutations remain serialized with movement callbacks.
template<class Session> class VerticalMoveRetainedInteractions final:public VerticalMovePvpInteractionServices {
public:
    using GuildRefresh=std::function<void(Actor&)>;
    VerticalMoveRetainedInteractions(VerticalSessionRegistry<Session>& registry,ZoneUsers& zone,
        const VerticalDuelMovement& duel,VerticalMovePvpServices& pvp,GuildRefresh guild)
        :VerticalMovePvpInteractionServices(duel,pvp),registry_(registry),zone_(zone),guild_(std::move(guild)) {
        if(!guild_)throw std::invalid_argument("Movement requires a guild-mark service");
    }
    std::shared_ptr<void> retain(Actor& actor) override {
        auto owner=registry_.retain_actor(actor);
        if(!owner)throw std::logic_error("Movement interaction actor has no retained session");
        return owner;
    }
    UserPosition position(const Actor& actor) override {
        auto owner=registry_.retain_actor(actor);
        if(!owner || zone_.find_actor(actor.id())!=&actor)throw std::logic_error("Movement interaction actor left its retained zone");
        auto position=zone_.position(actor.id());if(!position)throw std::logic_error("Movement interaction actor has no position");
        return *position;
    }
    void refresh_guild_mark(Actor& actor) override {
        auto owner=retain(actor);guild_(actor);
    }
private:
    VerticalSessionRegistry<Session>& registry_;ZoneUsers& zone_;GuildRefresh guild_;
};
}
