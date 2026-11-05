#pragma once
#include "eternity/game/vertical_move_packet.hpp"
#include "eternity/game/user_priority_zone_view.hpp"

namespace eternity::game {
// Binds PacketPC's prioritized movement send to the actor's shared PSendView
// cadence and actual zone traversal. The same state must be shared with all
// other calls to CUser::PSendView for this actor; services outlive callbacks.
// Derived world owner still implements clock, status-stand, correction, speed
// table, remaining movement effects, and close handling.
class VerticalMovePriorityServices:public VerticalMovePacketServices {
public:
    VerticalMovePriorityServices(UserPriorityViewState& state,UserPriorityZoneView& view):state_(state),view_(view){}
    bool send_priority_view(Actor& actor,const net::Packet& packet,std::uint8_t priority) final {
        return user_priority_view(actor,state_,packet,0,priority,view_);
    }
private:UserPriorityViewState& state_;UserPriorityZoneView& view_;
};
}
