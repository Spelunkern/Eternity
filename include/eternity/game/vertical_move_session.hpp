#pragma once
#include "eternity/game/vertical_move_packet.hpp"
#include "eternity/game/world_admission.hpp"
#include <optional>

namespace eternity::game {
// Per-connection adapter for a retained actor and its exact admission record.
// The world owner dispatches on its serialized executor and keeps every borrowed
// object alive through callbacks. PacketPC's unresolved provider contracts and
// the non-reentrant movement limitation still apply.
class VerticalMoveSession final {
public:
    VerticalMoveSession(Actor& actor,RuntimeZone& zone,WorldAdmissionUser& user,ZoneEntryState& entry,
        VerticalMotionState& motion,UserViewActorTransport& view,VerticalMovePacketServices& services)
        :actor_(actor),zone_(zone),user_(user),entry_(entry),motion_(motion),view_(view),services_(services){}
    // Nullopt means a different opcode; malformed 0501 is rejected by the
    // reconstructed packet handler. Correction may modify only this owned copy.
    std::optional<VerticalMoveResult> receive(std::span<const std::uint8_t> packet) {
        if(packet.size()<2 || packet[0]!=1 || packet[1]!=5)return {};
        if(user_.zone!=&zone_ || user_.user_id!=actor_.account_id())
            throw std::logic_error("Movement session lost actor or zone ownership");
        net::Packet mutable_packet(packet.begin(),packet.end());
        return vertical_move_packet(actor_,zone_,entry_,user_.location,motion_,mutable_packet,view_,services_);
    }
private:
    Actor& actor_;RuntimeZone& zone_;WorldAdmissionUser& user_;ZoneEntryState& entry_;VerticalMotionState& motion_;
    UserViewActorTransport& view_;VerticalMovePacketServices& services_;
};
}
