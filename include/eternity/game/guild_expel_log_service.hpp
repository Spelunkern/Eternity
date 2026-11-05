#pragma once
#include "eternity/game/guild_npc_log.hpp"
#include "eternity/game/actor.hpp"
#include "eternity/game/money.hpp"
#include <memory>

namespace eternity::game {
// CGuild::JoinExpel 0042de83..0042df84. The original copies the requester's
// character name at offset 6. For an online target a 19/20-byte name overlaps
// its already-written character ID; for offline targets the ID is written later.
inline std::function<void(Actor&,const GuildMember&,GuildUser*,const LoadedGuild&)> guild_expel_log_service(
    MoneyWorld& world,std::function<std::shared_ptr<Actor>(GuildUser&)> resolve_actor) {
    if(!resolve_actor)throw std::invalid_argument("Missing expelled actor resolver");
    return [&world,resolve_actor=std::move(resolve_actor)](Actor& requester,const GuildMember& record,GuildUser* peer,const LoadedGuild& guild) {
        const auto& name=requester.appearance().identity.name;
        if(name.size()>20 || name.find('\0')!=name.npos)throw std::invalid_argument("Invalid expelling character name");
        GameLogSubject subject;std::shared_ptr<Actor> actor;
        if(peer) {
            if(peer->character_id!=record.id)throw std::logic_error("Expelled member identity mismatch");
            actor=resolve_actor(*peer);if(!actor || actor->id()!=record.id)throw std::logic_error("Expelled actor unavailable");
            subject=world.money_log_subject(*actor);
            if(subject.character_id!=record.id || subject.account_id!=actor->account_id())throw std::logic_error("Expelled log subject mismatch");
        } else {subject.character_id=record.id;subject.character_name=record.name;}
        const auto time=world.money_local_calendar();const GuildNpcLogUser blank{};auto packet=guild_house_log_packet(blank,guild);packet[0]=6;
        write_game_log_main(packet,subject,time);std::copy(name.begin(),name.end(),packet.begin()+6);packet[6+name.size()]=0;
        if(!peer)for(unsigned i=0;i<4;++i)packet[25+i]=static_cast<std::uint8_t>(record.id>>(8*i));
        world.send_money_log(packet);
    };
}
}
