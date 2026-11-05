#pragma once
#include "eternity/game/actor.hpp"
#include "eternity/game/party_runtime_requests.hpp"

namespace eternity::game {
// CUser+1754/+5838 and +5478/+583c. Names/semantics beyond leave are not
// assumed; the owning session retains the same fields used by packet handlers.
struct ZoneLeaveRequestState {
    std::uint32_t request_1754=0,deadline_5838=0;
    std::uint32_t request_5478=0,deadline_583c=0;
};
// CUser::UpdateTimeover 0048e3be..0048e3e0: unsigned absolute tick,
// and a zero deadline or missing requester leaves both stored fields intact.
inline bool zone_guild_invitation_timeout(ZoneLeaveRequestState& state,std::uint32_t tick) noexcept {
    if(!state.deadline_5838 || tick<state.deadline_5838 || !state.request_1754)return false;
    state.request_1754=state.deadline_5838=0;return true;
}
class ZoneLeaveRequestServices {
public:
    virtual ~ZoneLeaveRequestServices()=default;
    virtual Actor* find_in_zone(Actor&,std::uint32_t)=0;
    virtual std::shared_ptr<void> retain(Actor&)=0;
};
// ps_game.exe CUser::LeaveZone 0044b04c..0044b19c. World owner serializes all
// state and retains subject/zone; returned peer lease survives each send.
inline void zone_leave_requests(Actor& actor,PartyRequestState& party,ZoneLeaveRequestState& state,ZoneLeaveRequestServices& services) {
    auto& action=actor.actions();const auto mode=actor.appearance().vehicle_mode;
    const auto find=[&](std::uint32_t id,auto&& callback) {
        if(auto* peer=services.find_in_zone(actor,id)) {
            auto lease=services.retain(*peer);if(!lease)throw std::logic_error("Cannot retain request peer");callback(*peer);
        }
    };
    if(action.vehicle_invitation && (mode==0 || mode==2)) {
        find(action.vehicle_invitation,[&](Actor& peer){
            if(mode==2)peer.actions().vehicle_invitation=0;
            else if(peer.appearance().vehicle_mode==2) {
                peer.actions().vehicle_invitation=peer.actions().vehicle_invitation_until=0;peer.send_self({0x1e,2,2});
            }
        });
        if(mode==2)action.vehicle_invitation_until=0;
        action.vehicle_invitation=0;
    }
    if(party.inviter_id) {
        find(party.inviter_id,[&](Actor& peer){net::Packet packet{3,0x0b,1};for(unsigned i=0;i<4;++i)packet.push_back(static_cast<std::uint8_t>(actor.id()>>(8*i)));peer.send_self(packet);});
        party.inviter_id=party.invitation_deadline=0;
    }
    if(state.request_1754) {
        const auto id=state.request_1754;
        find(id,[&](Actor& peer){net::Packet packet{6,0x0d,0};for(unsigned i=0;i<4;++i)packet.push_back(static_cast<std::uint8_t>(id>>(8*i)));peer.send_self(packet);});
        state.request_1754=state.deadline_5838=0;
    }
    if(state.request_5478) {
        find(state.request_5478,[&](Actor& peer){peer.send_self({3,0x22,0});});
        state.request_5478=state.deadline_583c=0;
    }
}
}
