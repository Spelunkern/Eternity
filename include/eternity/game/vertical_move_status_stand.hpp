#pragma once
#include "eternity/game/vertical_database_ack.hpp"
#include "eternity/game/zone_shop_shutdown.hpp"

namespace eternity::game {
// ps_game.exe CUser::StatusStand 0048a100. This binding uses the same world
// admission, socket/transfer state and shop owner as disconnect and shop paths.
// The serialized world owner retains all objects through shutdown callbacks.
inline void vertical_move_status_stand(Actor& actor,VerticalDatabaseAckUser& connection,
    ZoneLeaveShopState& shop,ZoneShopShutdownServices& services) {
    auto& world=connection.user;
    if(&world.actor!=&actor || world.admission.user_id!=actor.account_id())
        throw std::logic_error("StatusStand requires the actor's retained connection");
    if(actor.presentation_motion()!=1)return;
    auto& user=world.admission;
    if(user.logout_type!=0 && connection.socket_open && connection.transfer_state!=1) {
        user.logout_tick=0;user.logout_type=0;
    }
    actor.presentation_motion(0);
    if(actor.attack_state().recovery<1)actor.attack_state().recovery=1;
    zone_shutdown_shop(actor,shop,services);
}
}
