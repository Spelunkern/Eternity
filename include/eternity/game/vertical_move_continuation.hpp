#pragma once
#include "eternity/game/zone_entry.hpp"
#include <span>

namespace eternity::game {
// Canonical per-character CUser+57cc and +1435 fields; owner retains this
// alongside ZoneEntryState. +1434 presentation motion is a different field.
struct VerticalMotionState {std::uint32_t last_tick=0;std::uint8_t motion=0;};
class VerticalMoveContinuationServices {
public:
    virtual ~VerticalMoveContinuationServices()=default;
    virtual std::uint32_t tick_ms()=0;
    virtual bool send_priority_view(Actor&,const net::Packet&,std::uint8_t priority)=0;
    // 0046a55f..0046a64d: guild mark, interaction distance, trade and PK.
    // This service must implement the original complete remaining effects.
    // Boolean is PSendView's periodic wide-pass result, not send success.
    virtual void finish_move(Actor&,bool wide_pass)=0;
};
// CUser::StatusMove 0048a090, reuses canonical attack, target and action state.
inline void vertical_status_move(Actor& actor,const std::function<std::uint32_t()>& tick) {
    auto& attack=actor.attack_state();if(attack.mode!=3)return;
    auto& status=actor.status();const auto& action=actor.actions();
    if(status.current_target_type==1 && status.current_target==actor.id()) {
        status.current_target_type=action.previous_target_type;status.current_target=action.previous_target;
    }
    if(attack.previous_tick==0) {actor.stop_attack(status);return;}
    if(!tick)throw std::invalid_argument("StatusMove requires a clock");
    attack.mode=2;attack.next_tick=1;attack.resume_tick=tick();
}
// PacketPC 0046a49d..0046a64d, after successful MoveUserByPacket. The owner
// retains and serializes actor, input storage, state and all services; callbacks
// may alter live actor state, and packet coordinates are read after callbacks.
inline void vertical_move_continuation(Actor& actor,ZoneEntryState& entry,VerticalMotionState& state,
    std::span<const std::uint8_t> packet,VerticalMoveContinuationServices& services) {
    if(packet.size()<17 || packet[0]!=1 || packet[1]!=5 || !entry.bound || entry.character_id!=actor.id())
        throw std::invalid_argument("Movement continuation requires admitted actor and 0501 packet");
    entry.direction=static_cast<std::uint16_t>(packet[3]|(static_cast<unsigned>(packet[4])<<8));
    state.last_tick=services.tick_ms();state.motion=packet[2];
    if(packet[2]&0x80)actor.stop_attack(actor.status());else vertical_status_move(actor,[&]{return services.tick_ms();});
    if(actor.appearance().vehicle_mode==1) {actor.appearance().vehicle_mode=0;actor.actions().vehicle_request=0;}
    if(actor.attack_state().motion)state.motion|=0x80;
    net::Packet output{1,5};const auto id=actor.id();for(unsigned shift=0;shift<32;shift+=8)output.push_back(static_cast<std::uint8_t>(id>>shift));
    output.push_back(state.motion);output.push_back(static_cast<std::uint8_t>(entry.direction));output.push_back(static_cast<std::uint8_t>(entry.direction>>8));
    output.insert(output.end(),packet.begin()+5,packet.begin()+17);
    const bool wide_pass=services.send_priority_view(actor,output,7);services.finish_move(actor,wide_pass);
}
}
