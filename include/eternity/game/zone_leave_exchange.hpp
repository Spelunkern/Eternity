#pragma once
#include "eternity/game/actor.hpp"
#include <functional>

namespace eternity::game {
// 0044af3c..0044b04c of ps_game.exe CUser::LeaveZone 0044aee0. Invoked after
// the NPC/other preceding cleanup, before the vehicle/invitation/duel cleanup.
// The actor executor serializes exchange and timer state; pin keeps the peer and
// its scene alive during callbacks. The actual owner retains timer CUser+5830.
inline void zone_leave_exchange(Actor& actor,const std::function<std::shared_ptr<void>(Actor&)>& pin,
    const std::function<std::uint32_t&(Actor&)>& timer) {
    if(!pin || !timer)throw std::invalid_argument("Exchange leave requires owner services");
    auto& local=actor.exchange();const auto mode=local.mode;if(mode==0)return;
    Actor* peer=local.peer;std::shared_ptr<void> lease;
    if(peer){lease=pin(*peer);if(!lease)throw std::logic_error("Exchange peer has no owner lease");}
    if(mode==1 || mode==2) {
        if(peer && peer->exchange().mode==1) {
            peer->exchange()={};
            if(mode==1)timer(*peer)=0;
            else {
                net::Packet packet{3,10};for(unsigned i=0;i<4;++i)packet.push_back(static_cast<std::uint8_t>(actor.id()>>(i*8)));
                packet.push_back(1);peer->send_self(packet);
            }
        }
        local={};if(mode==2)timer(actor)=0;
    } else if(peer) {
        local={};peer->exchange()={};actor.send_self({4,10,2});peer->send_self({4,10,3});
    }
}
}
