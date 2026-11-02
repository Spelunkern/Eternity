#include "eternity/game/actor.hpp"

namespace eternity::game {
void Actor::cancel_move(ActorActionWorld& world) {
    // ps_game.exe CUser::CancleMove 0048a010.
    if(actions_.pending_move==0) return;
    if(actions_.move_type==0) {actions_.pending_move=0;return;}
    if(actions_.move_type<2) return;
    actions_.pending_move=0;
    if(actions_.move_slot<actions_.move_slots.size()) actions_.move_slots[actions_.move_slot]=0;
    if(status_.in_zone) {
        net::Packet packet{0x22,2};for(unsigned int i=0;i<4;++i) packet.push_back(static_cast<std::uint8_t>(id_>>(8*i)));
        packet.push_back(0);world.send_move_cancel(*this,packet);
    }
}
void Actor::cancel_vehicle(ActorActionWorld& world) {
    // CancleActionVehicle 0044b2c0. Peer notifications precede clearing the
    // caller's partner field; lookup and packet callbacks can observe that order.
    if(appearance_.vehicle_mode!=2) return;
    if(appearance_.vehicle_partner!=0) {
        if(auto* peer=world.find_user(*this,appearance_.vehicle_partner)) {
            peer->appearance_.vehicle_partner=0;peer->appearance_.vehicle_mode=0;peer->send_vehicle_partner();
        }
        appearance_.vehicle_partner=0;
    }
    if(actions_.vehicle_invitation!=0) {
        if(auto* peer=world.find_user(*this,actions_.vehicle_invitation)) peer->actions_.vehicle_invitation=0;
        actions_.vehicle_invitation=0;actions_.vehicle_invitation_until=0;
    }
}
void Actor::stop_casting(ActorActionWorld& world) {
    // StatusCastingStop 00489f10: vehicle, pending move, shape, sleep, then
    // target restoration and either attack-stop or attack-resume scheduling.
    if(appearance_.vehicle_mode==1) {actions_.vehicle_request=0;appearance_.vehicle_mode=0;}
    else if(appearance_.vehicle_mode==2) {cancel_vehicle(world);appearance_.vehicle_mode=0;shape_changed(status_);}
    else if(appearance_.vehicle_mode==3) {
        appearance_.vehicle_mode=0;send_vehicle_partner();
        if(auto* peer=world.find_user(*this,appearance_.vehicle_partner)) {
            peer->appearance_.vehicle_partner=0;peer->appearance_.vehicle_mode=0;peer->shape_changed(peer->status_);
        }
        appearance_.vehicle_partner=0;
    }
    cancel_move(world);cancel_shape_on_attack();remove_sleep();
    if(attack_.mode!=3) return;
    if(status_.current_target_type==1 && status_.current_target==id_) {
        status_.current_target_type=actions_.previous_target_type;status_.current_target=actions_.previous_target;
    }
    if(attack_.previous_tick==0) {stop_attack(status_);return;}
    attack_.mode=2;attack_.next_tick=1;attack_.resume_tick=tick_ms();
}
}
