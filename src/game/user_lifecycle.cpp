#include "eternity/game/user_lifecycle.hpp"

namespace eternity::game {
UserLifecycle::UserLifecycle(UserLifecycleState& state,UserLifecyclePorts ports):state_(state),ports_(std::move(ports)) {
    if(!ports_.post || !ports_.send || !ports_.leave || !ports_.reset || !ports_.roster || !ports_.save || !ports_.save_exit) throw std::invalid_argument("Incomplete DB user lifecycle ports");
}
void UserLifecycle::notify(std::uint16_t opcode) {
    if(!state_.connected) return;
    net::Packet packet{static_cast<std::uint8_t>(opcode),static_cast<std::uint8_t>(opcode>>8)};
    for(unsigned int i=0;i<4;++i) packet.push_back(static_cast<std::uint8_t>(state_.user_id>>(i*8)));
    ports_.send(std::move(packet));
}
bool UserLifecycle::update_save(std::uint32_t now,std::uint32_t& last) {return update_character_save_tick(state_.status,now,state_.next_save_tick,last,ports_.post);}
void UserLifecycle::character_loaded(std::uint32_t save_time_id,std::uint32_t last) {state_.status=2;state_.next_save_tick=last+(save_time_id%900U)*1000U;}
void UserLifecycle::exit() {notify(0x302);if(state_.status==2) notify(0x303);ports_.leave();}
void UserLifecycle::disconnect_by_game_server() {if(state_.status==2) ports_.post(9);else ports_.leave();}
void UserLifecycle::logout_select() {if(state_.status==2) ports_.post(10);else {notify(0x302);ports_.leave();}}
bool UserLifecycle::dispatch_save_job(std::uint32_t job) {
    switch(job) {
    case 8:if(state_.status==2) (void)ports_.save(true,false);break;
    case 9:case 22:if(state_.status==1) exit();else if(state_.status==2) ports_.save_exit(job==22);break;
    case 10:
        if(state_.status==2 && ports_.save(false,true)) {
            notify(0x303);state_.status=1;state_.next_save_tick=0;ports_.reset();ports_.roster(true);
        }
        break;
    default:return false;
    }
    return true;
}
}
