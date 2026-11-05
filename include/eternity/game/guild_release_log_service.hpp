#pragma once
#include "eternity/game/guild_npc_log.hpp"
#include "eternity/game/actor.hpp"
#include "eternity/game/money.hpp"

namespace eternity::game {
// CGuild::Release 0042d5e1..0042d659: the acting user is optional, and the
// separate clock read overwrites SetGameLogMain's timestamp before sending.
inline std::function<void(Actor*,const LoadedGuild&)> guild_release_log_service(MoneyWorld& world) {
    return [&world](Actor* actor,const LoadedGuild& guild) {
        const GuildNpcLogUser blank{};auto packet=guild_house_log_packet(blank,guild);packet[0]=2;
        if(actor) {
            const auto subject=world.money_log_subject(*actor);
            if(subject.account_id!=actor->account_id() || subject.character_id!=actor->id())throw std::logic_error("Guild release log subject mismatch");
            write_game_log_main(packet,subject,world.money_local_calendar());
        }
        std::array<std::uint8_t,74> time_header{};write_game_log_main(time_header,{},world.money_local_calendar());
        std::copy_n(time_header.begin()+70,4,packet.begin()+70);world.send_money_log(packet);
    };
}
}
