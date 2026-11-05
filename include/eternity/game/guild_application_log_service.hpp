#pragma once
#include "eternity/game/guild_npc_log.hpp"
#include "eternity/game/actor.hpp"
#include "eternity/game/money.hpp"
#include <memory>

namespace eternity::game {
// Resolve an online applicant to an aliasing lease that retains its actual
// session and Actor. MoneyWorld owns the real local clock and log transport.
// ps_game.exe CGuild::JoinOK 0042e535..0042e60a, SetGameLogMain 0045b0d0.
inline std::function<void(const GuildMember&,GuildUser*,const LoadedGuild&)> guild_application_log_service(
    MoneyWorld& world,std::function<std::shared_ptr<Actor>(GuildUser&)> resolve_actor) {
    if(!resolve_actor)throw std::invalid_argument("Missing guild applicant actor resolver");
    return [&world,resolve_actor=std::move(resolve_actor)](const GuildMember& record,GuildUser* peer,const LoadedGuild& guild) {
        GameLogSubject subject;
        std::shared_ptr<Actor> actor;
        if(peer) {
            if(peer->character_id!=record.id)throw std::logic_error("Guild applicant identity mismatch");
            actor=resolve_actor(*peer);
            if(!actor || actor->id()!=record.id)throw std::logic_error("Guild applicant actor unavailable");
            subject=world.money_log_subject(*actor);
            if(subject.character_id!=record.id || subject.account_id!=actor->account_id())throw std::logic_error("Guild applicant log subject mismatch");
        } else {subject.character_id=record.id;subject.character_name=record.name;}
        const auto time=world.money_local_calendar();
        const GuildNpcLogUser blank{};auto packet=guild_house_log_packet(blank,guild);packet[0]=4;
        write_game_log_main(packet,subject,time);world.send_money_log(packet);
    };
}
}
