#pragma once
#include "eternity/game/vertical_move_pvp.hpp"
#include "eternity/game/duel.hpp"
#include "eternity/game/game_log.hpp"
#include <algorithm>
#include <array>

namespace eternity::game {
// CUser::PvPChkLeaveByMove 0047c00f..0047c160. The owner supplies the live
// DuelWorld, lifetime pinning and initial bytes of the original log stack
// buffer, whose untouched bytes are otherwise indeterminate. All actors,
// world resources and callbacks are serialized and retained through this call.
inline void vertical_leave_active_duel(Actor& actor,DuelWorld& world,
    const std::function<std::shared_ptr<void>(Actor&)>& retain,std::array<std::uint8_t,100> log_storage) {
    if(!retain)throw std::invalid_argument("Duel movement requires peer retention");
    net::Packet result{6,0x24,1};const auto id=actor.id();
    for(unsigned shift=0;shift<32;shift+=8)result.push_back(static_cast<std::uint8_t>(id>>shift));
    Actor* peer=world.find_duel_user(actor,actor.relations().duel_opponent);std::shared_ptr<void> pin;
    if(peer) {
        pin=retain(*peer);if(!pin)throw std::logic_error("Cannot retain active duel peer");
        drop_duel_wager(*peer,*peer,world);peer->end_duel();peer->send_self(result);
    }
    drop_duel_wager(actor,actor,world);actor.end_duel();actor.send_self(result);
    net::Packet log(log_storage.begin(),log_storage.end());log[0]=3;log[1]=0x0e;log[99]=1;
    const auto emit=[&](Actor& subject,const Actor* target) {
        write_game_log_main(log,world.duel_log_subject(subject),world.local_calendar());
        if(target) {
            const auto detail=world.duel_log_subject(*target);
            if(detail.character_name.size()>20 || detail.character_name.find('\0')!=std::string::npos)
                throw std::invalid_argument("Invalid movement duel log target name");
            for(unsigned i=0;i<4;++i)log[74+i]=static_cast<std::uint8_t>(detail.character_id>>(8*i));
            std::copy(detail.character_name.begin(),detail.character_name.end(),log.begin()+78);log[78+detail.character_name.size()]=0;
        } else {std::fill_n(log.begin()+74,4,0);log[78]=0;}
        world.send_duel_log(log);
    };
    emit(actor,peer);if(peer)emit(*peer,&actor);
}
// Provides the recovered individual-duel work over the real DuelWorld; guild
// duel checking, actor lifetime and log-stack policy remain required owners.
class VerticalMovePvpWorldServices:public VerticalMovePvpServices {
public:
    explicit VerticalMovePvpWorldServices(DuelWorld& world):world_(world){}
    UserPosition position(const Actor& actor) final{return world_.duel_position(actor);}
    Actor* find_opponent(Actor& actor,std::uint32_t id) final{return world_.find_duel_user(actor,id);}
    void end_duel(Actor& actor) final{actor.end_duel();}
    void leave_active_duel(Actor& actor) final {
        vertical_leave_active_duel(actor,world_,[&](Actor& subject){return retain(subject);},log_stack_bytes());
    }
    virtual std::array<std::uint8_t,100> log_stack_bytes()=0;
private:DuelWorld& world_;
};
}
