#pragma once
#include "eternity/game/actor.hpp"
#include "eternity/game/duel.hpp"
#include <algorithm>
#include <array>

namespace eternity::game {
// Individual-duel half of ps_game.exe PvPChkLeaveByZone 0047b920..0047bafb.
// The caller then executes its guild-duel half. The log buffer preserves the
// original untouched stack bytes; peers and their scenes are retained by pin.
inline void zone_leave_individual_duel(Actor& actor,DuelWorld& world,
    const std::function<std::shared_ptr<void>(Actor&)>& pin,std::array<std::uint8_t,100> scratch) {
    if(!pin)throw std::invalid_argument("Zone duel leave requires retention");
    const auto state=actor.relations().duel_state;if(state==0)return;
    Actor* peer=world.find_duel_user(actor,actor.relations().duel_opponent);if(!peer)return;
    auto lease=pin(*peer);if(!lease)throw std::logic_error("Cannot retain duel peer");
    if(state<1 || state>5)return;
    const bool wager=state==3 || state==4;
    if(wager){drop_duel_wager(*peer,*peer,world);drop_duel_wager(actor,actor,world);}
    peer->end_duel();actor.end_duel();
    if(state==5){actor.send_self({8,0x24,2});peer->send_self({8,0x24,3});return;}
    const auto id=state==1?peer->id():actor.id();net::Packet packet{static_cast<std::uint8_t>(wager?6:2),0x24,static_cast<std::uint8_t>(wager?0:3)};
    for(unsigned i=0;i<4;++i)packet.push_back(static_cast<std::uint8_t>(id>>(i*8)));
    peer->send_self(packet);
    if(!wager){actor.send_self(packet);return;}
    net::Packet log(scratch.begin(),scratch.end());log[0]=3;log[1]=0x0e;log[99]=0;
    const auto emit=[&](Actor& subject,Actor& other) {
        write_game_log_main(log,world.duel_log_subject(subject),world.local_calendar());const auto target=world.duel_log_subject(other);
        if(target.character_name.size()>20 || target.character_name.find('\0')!=std::string::npos)throw std::invalid_argument("Invalid duel log name");
        for(unsigned i=0;i<4;++i)log[74+i]=static_cast<std::uint8_t>(target.character_id>>(8*i));
        std::copy(target.character_name.begin(),target.character_name.end(),log.begin()+78);log[78+target.character_name.size()]=0;world.send_duel_log(log);
    };
    emit(*peer,actor);emit(actor,*peer);
}
}
