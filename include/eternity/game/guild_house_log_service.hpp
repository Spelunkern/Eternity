#pragma once
#include "eternity/game/guild_npc_log.hpp"
#include "eternity/game/actor.hpp"
#include "eternity/game/money.hpp"

namespace eternity::game {
// Retain actor, catalog and MoneyWorld across the callback. MoneyWorld supplies
// the actual actor log subject, original local calendar and game-log transport.
// ps_game.exe PacketGuild 004772d8..00477325 / SetGameLogMain 0045b0d0.
inline std::function<void(GuildUser&,const LoadedGuild&)> guild_house_log_service(
    Actor& actor,GuildCatalog& catalog,MoneyWorld& world) {
    return [&actor,&catalog,&world](GuildUser& user,const LoadedGuild&) {
        if(user.character_id!=actor.id())throw std::logic_error("Guild house log actor mismatch");
        const auto subject=world.money_log_subject(actor);
        if(subject.account_id!=actor.account_id() || subject.character_id!=actor.id())throw std::logic_error("Guild house log subject mismatch");
        const auto time=world.money_local_calendar();
        if(!user.guild)throw std::logic_error("Guild house log owner removed");
        const auto* guild=catalog.find_membership(user.guild);
        if(!guild)throw std::logic_error("Foreign guild house log owner");
        const GuildNpcLogUser blank{};
        auto packet=guild_house_log_packet(blank,*guild);
        write_game_log_main(packet,subject,time);
        world.send_money_log(packet);
    };
}
}
