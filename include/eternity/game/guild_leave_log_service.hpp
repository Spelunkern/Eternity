#pragma once
#include "eternity/game/guild_npc_log.hpp"
#include "eternity/game/actor.hpp"
#include "eternity/game/money.hpp"

namespace eternity::game {
// CUser::LeaveGuild 0042dbef..0042dc2f logs the captured guild after clearing
// CUser+1758. MoneyWorld supplies the real subject, calendar and log transport.
inline std::function<void(GuildUser&,Actor&,const LoadedGuild&)> guild_leave_log_service(MoneyWorld& world) {
    return [&world](GuildUser& user,Actor& actor,const LoadedGuild& guild) {
        if(user.character_id!=actor.id())throw std::logic_error("Guild leave log actor mismatch");
        const auto subject=world.money_log_subject(actor);
        if(subject.account_id!=actor.account_id() || subject.character_id!=actor.id())throw std::logic_error("Guild leave log subject mismatch");
        const auto time=world.money_local_calendar();const GuildNpcLogUser blank{};
        auto packet=guild_house_log_packet(blank,guild);packet[0]=5;
        write_game_log_main(packet,subject,time);world.send_money_log(packet);
    };
}
}
