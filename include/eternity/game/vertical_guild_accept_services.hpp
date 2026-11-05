#pragma once
#include "eternity/game/guild_client_accept.hpp"
#include "eternity/game/guild_application_log_service.hpp"
#include "eternity/game/vertical_world_session.hpp"
#include "eternity/game/world_users.hpp"

namespace eternity::game {
// All owners outlive the returned synchronous service bundle. The state pins
// the exact online session from lookup through all client, DB and log callbacks.
inline GuildApplicationAcceptPorts vertical_guild_accept_services(VerticalWorldSession& requester,
    VerticalWorldSession::Registry& registry,WorldUsers& world,MoneyWorld& money,
    std::function<void(net::Packet)> send_db) {
    if(!send_db)throw std::invalid_argument("Missing guild acceptance DB transport");
    struct State {std::shared_ptr<VerticalWorldSession> peer;};auto state=std::make_shared<State>();
    auto resolve=[state](GuildUser& user)->std::shared_ptr<Actor> {
        if(!state->peer || &state->peer->guild_user()!=&user || !state->peer->world_user())throw std::logic_error("Guild applicant lost retained actor");
        auto& actor=state->peer->world_user()->actor;
        if(actor.id()!=user.character_id)throw std::logic_error("Guild applicant actor mismatch");
        return std::shared_ptr<Actor>(state->peer,&actor);
    };
    auto send=[state](GuildUser& user,net::Packet packet) {
        if(!state->peer || &state->peer->guild_user()!=&user || !state->peer->channel().send)throw std::logic_error("Guild applicant lost client transport");
        state->peer->channel().send(std::move(packet));
    };
    GuildApplicationAcceptPorts result;
    result.find_online=[state,&world,&registry](std::uint32_t id)->std::shared_ptr<GuildUser> {
        state->peer.reset();auto* found=world.find(id);if(!found)return {};
        state->peer=registry.retain_actor(found->actor);
        if(!state->peer || state->peer->world_user()!=found || state->peer->guild_user().character_id!=id)throw std::logic_error("Guild applicant lost session owner");
        return std::shared_ptr<GuildUser>(state->peer,&state->peer->guild_user());
    };
    result.online_level=[resolve](GuildUser& user){return resolve(user)->status().level;};
    result.send_user=send;result.send_db=std::move(send_db);
    result.send_bank=[send](GuildUser& user,const LoadedGuild& guild) {
        if(!guild.bank)throw std::logic_error("Guild acceptance requires retained bank");
        for(auto& packet:guild.bank->client_packets())send(user,std::move(packet));
    };
    result.tick=[&requester] {if(!requester.world_user())throw std::logic_error("Guild acceptance lost requester actor");return requester.world_user()->actor.tick_ms();};
    result.set_join_deadline=[state](GuildUser& user,std::uint32_t deadline) {
        if(!state->peer || &state->peer->guild_user()!=&user)throw std::logic_error("Guild applicant lost deadline owner");user.join_deadline=deadline;
    };
    result.game_log=guild_application_log_service(money,std::move(resolve));return result;
}
}
