#pragma once
#include "eternity/game/vertical_movement_prefix.hpp"
#include "eternity/game/vertical_move_user.hpp"
#include "eternity/game/vertical_move_continuation.hpp"
#include <bit>
#include <algorithm>
#include <array>

namespace eternity::game {
enum class VerticalMoveResult {life_blocked,distance_corrected,speed_corrected,move_rejected,moved};
// Original table at 00542508 is writable process data; owner retains its live
// values rather than assuming defaults cannot change. 0046a36d..0046a3a8 uses
// uncloaked equipment speed index, independent of movement-block flags.
inline float vertical_movement_selected_speed(const Actor& actor,const std::array<float,5>& table) {
    const auto mode=actor.appearance().vehicle_mode;
    if(mode==2)return 8.0F;
    if(mode==3)return static_cast<float>(static_cast<double>(table[4])+10000.0);
    return table[static_cast<std::size_t>(std::clamp(actor.status().skills.equipment.move_speed,0,4))];
}
// Owner supplies the actual live table and clock used for CheckMove.
// Other callbacks own the real status,
// prioritized view, remaining interaction effects and connection close.
class VerticalMoveWorldOperations:public VerticalMovementPrefixServices {
public:
    virtual std::uint32_t tick_ms()=0;
    virtual const std::array<float,5>& movement_speed_table()=0;
    virtual void close(Actor&,std::uint32_t)=0;
};
class VerticalMovePacketServices:public VerticalMoveWorldOperations,public VerticalMoveContinuationServices {
public:
    // One clock supplies both the packet gate and continuation interfaces.
    virtual std::uint32_t tick_ms() override=0;
};
// Connects reconstructed PacketPC stages; all underlying contracts, including
// non-reentrant subject movement and retained packet/zone/session, apply.
inline VerticalMoveResult vertical_move_packet(Actor& actor,RuntimeZone& zone,ZoneEntryState& admission,
    std::uint32_t location,VerticalMotionState& motion,std::span<std::uint8_t> packet,
    UserViewActorTransport& view,VerticalMovePacketServices& services) {
    const auto prefix=vertical_movement_prefix(actor,zone,admission,packet,services);
    if(prefix==VerticalMovementPrefixResult::life_blocked)return VerticalMoveResult::life_blocked;
    if(prefix==VerticalMovementPrefixResult::distance_corrected)return VerticalMoveResult::distance_corrected;
    const auto speed=vertical_movement_selected_speed(actor,services.movement_speed_table());
    const auto checked=vertical_movement_packet_check_after_gate(actor,zone,admission,actor.movement_check(),packet,speed,[&]{return services.tick_ms();});
    if(!checked.accepted_by_speed_check) {
        vertical_movement_speed_correction(actor,zone,admission,packet,services);
        return VerticalMoveResult::speed_corrected;
    }
    // Original rereads packet coordinates after CheckMove and its clock calls.
    const auto coordinate=[&](std::size_t offset){std::uint32_t bits=0;for(unsigned i=0;i<4;++i)bits|=static_cast<std::uint32_t>(packet[offset+i])<<(8*i);return std::bit_cast<float>(bits);};
    const UserPosition destination{coordinate(5),coordinate(9),coordinate(13)};
    if(!vertical_move_user(actor,zone,admission,location,destination,view,[&](auto reason){services.close(actor,reason);}))
        return VerticalMoveResult::move_rejected;
    vertical_move_continuation(actor,admission,motion,packet,services);return VerticalMoveResult::moved;
}
}
