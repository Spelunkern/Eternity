#pragma once
#include "eternity/game/guild_client_expel_online.hpp"
#include "eternity/game/guild_expel_log_service.hpp"
#include "eternity/game/vertical_world_session.hpp"
#include "eternity/game/world_users.hpp"

namespace eternity::game {
// Pins the actual target session from lookup through target send, duel and log.
inline GuildExpelOnlinePorts vertical_guild_expel_services(VerticalWorldSession& requester,
    VerticalWorldSession::Registry& registry,WorldUsers& world,MoneyWorld& money,
    std::function<void(net::Packet)> send_db,
    std::function<ZoneLeaveGuildDuelServices&(VerticalWorldSession&)> duel_for) {
    if(!send_db || !duel_for)throw std::invalid_argument("Missing online guild expulsion owners");
    struct State {std::shared_ptr<VerticalWorldSession> peer;};auto state=std::make_shared<State>();
    auto resolve=[state,&world,&registry](GuildUser& user)->std::shared_ptr<Actor> {
        if(!state->peer) {
            auto* found=world.find(user.character_id);if(!found)throw std::logic_error("Expelled actor unavailable");
            state->peer=registry.retain_actor(found->actor);
            if(!state->peer || state->peer->world_user()!=found || &state->peer->guild_user()!=&user || !state->peer->channel().send)
                throw std::logic_error("Expelled actor lost session owner");
        }
        if(&state->peer->guild_user()!=&user || !state->peer->world_user())throw std::logic_error("Expelled actor ownership changed");
        auto& actor=state->peer->world_user()->actor;if(actor.id()!=user.character_id)throw std::logic_error("Expelled actor identity mismatch");
        return std::shared_ptr<Actor>(state->peer,&actor);
    };
    GuildExpelOnlinePorts result;result.retain_actor=resolve;
    result.duel_for=[state,duel_for=std::move(duel_for)](GuildUser& peer)->ZoneLeaveGuildDuelServices& {
        if(!state->peer || &state->peer->guild_user()!=&peer)throw std::logic_error("Expelled duel owner unavailable");return duel_for(*state->peer);
    };
    result.send_target=[state](GuildUser& peer,net::Packet packet) {
        if(!state->peer || &state->peer->guild_user()!=&peer || !state->peer->channel().send)throw std::logic_error("Expelled client unavailable");
        state->peer->channel().send(std::move(packet));
    };
    result.send_user=[&requester](GuildUser& user,net::Packet packet) {
        if(&requester.guild_user()!=&user || !requester.channel().send)throw std::logic_error("Guild expulsion requester unavailable");
        requester.channel().send(std::move(packet));
    };
    result.send_db=std::move(send_db);result.game_log=guild_expel_log_service(money,std::move(resolve));return result;
}
}
