#pragma once
#include "eternity/game/guild_npc_log.hpp"
#include "eternity/game/actor.hpp"
#include "eternity/game/money.hpp"

namespace eternity::game {
// CGuild::JoinRequest 0042e29d; required MoneyWorld owns the live subject,
// original local calendar and game-log transport. Actor and guild outlive call.
inline std::function<void(GuildUser&,Actor&,const LoadedGuild&)> guild_application_request_log(MoneyWorld& world) {
    return [&world](GuildUser& user,Actor& actor,const LoadedGuild& guild) {
        if(user.character_id!=actor.id())throw std::logic_error("Guild application log actor mismatch");
        const auto subject=world.money_log_subject(actor);
        if(subject.account_id!=actor.account_id() || subject.character_id!=actor.id())throw std::logic_error("Guild application log subject mismatch");
        const auto time=world.money_local_calendar();const GuildNpcLogUser blank{};auto packet=guild_house_log_packet(blank,guild);packet[0]=3;
        write_game_log_main(packet,subject,time);world.send_money_log(packet);
    };
}
}
