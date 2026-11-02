#pragma once
#include "eternity/net/framing.hpp"
#include <array>
#include <cstdint>

namespace eternity::game {
class Actor;
struct ActorActionState {
    std::uint32_t vehicle_request=0,vehicle_invitation=0,vehicle_invitation_until=0; // +1464/+1474/+1478.
    std::int32_t move_type=0; // +5818.
    std::uint32_t pending_move=0,move_slot=0; // +581c/+586c.
    std::array<std::uint32_t,11> move_slots{}; // +1488; semantics beyond cancellation remain separate.
    std::int32_t previous_target_type=0; // +1530.
    std::uint32_t previous_target=0; // +1534.
};
class ActorActionWorld {
public:
    virtual ~ActorActionWorld() = default;
    // CZone::FindUser: resolve in the requester's zone, including its diagnostics.
    virtual Actor* find_user(Actor& requester,std::uint32_t id) = 0;
    // Ordinary CZone::SendView 004209a0, radius 60. Separate from PSendView.
    virtual void send_move_cancel(Actor&,const net::Packet&) = 0;
};
}
