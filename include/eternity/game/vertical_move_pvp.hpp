#pragma once
#include "eternity/game/actor.hpp"
#include <cmath>

namespace eternity::game {
// CUser+54d8, owned and kept current with the real duel lifecycle.
struct VerticalDuelMovement {UserPosition anchor;};
class VerticalMovePvpServices {
public:
    virtual ~VerticalMovePvpServices()=default;
    virtual UserPosition position(const Actor&)=0;
    virtual Actor* find_opponent(Actor&,std::uint32_t)=0;
    virtual std::shared_ptr<void> retain(Actor&)=0;
    // Required real CUser::PvPModeEnd (Actor::end_duel with live effects/scene).
    virtual void end_duel(Actor&)=0;
    // Required remaining state-3/4 wager, cleanup and log branch.
    virtual void leave_active_duel(Actor&)=0;
    // Required remaining guild-duel movement branch, always invoked last.
    virtual void check_guild_duel(Actor&)=0;
};
// CUser::PvPChkLeaveByMove 0047bef0. All borrowed owners and actors survive
// callbacks; active-duel and guild branches remain explicit real services.
inline void vertical_move_pvp(Actor& actor,const VerticalDuelMovement& state,VerticalMovePvpServices& services) {
    const auto duel=actor.relations().duel_state;
    if(duel==3 || duel==4 || duel==5) {
        const auto position=services.position(actor);
        const float dx=static_cast<float>(static_cast<double>(state.anchor.x)-position.x);
        const float dz=static_cast<float>(static_cast<double>(state.anchor.z)-position.z);
        const float distance=static_cast<float>(std::sqrt(static_cast<double>(dx)*dx+static_cast<double>(dz)*dz));
        if(distance>40.0F) {
            if(duel==5) {
                if(auto* peer=services.find_opponent(actor,actor.relations().duel_opponent)) {
                    auto pin=services.retain(*peer);if(!pin)throw std::logic_error("Cannot retain duel peer");
                    services.end_duel(*peer);peer->send_self({8,0x24,3});
                }
                services.end_duel(actor);actor.send_self({8,0x24,2});
            } else services.leave_active_duel(actor);
        }
    }
    services.check_guild_duel(actor);
}
}
