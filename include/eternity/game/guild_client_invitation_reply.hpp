#pragma once
#include "eternity/game/entry_catalogs.hpp"
#include "eternity/game/zone_leave_requests.hpp"
#include <span>

namespace eternity::game {
class GuildInvitationReplyServices:public ZoneLeaveRequestServices {
public:
    // Inspect the retained inviter's actual live guild and signed rank.
    virtual bool can_invite(Actor&)=0;
    // Returns false only when the real join dependencies are unavailable.
    virtual bool join(Actor&,Actor&)=0;
};
// ps_game.exe CUser::PacketGuild 00476d21..00476df7. The accepted-invitation
// branch requires the retained guild join owner and remains with the caller.
inline EntryCatalogUpdate guild_client_invitation_reply(Actor& actor,ZoneLeaveRequestState& state,
        std::span<const std::uint8_t> packet,GuildInvitationReplyServices* services) {
    if(packet.size()<3 || packet[0]!=6 || packet[1]!=0x0d)return EntryCatalogUpdate::rejected;
    if(!state.request_1754)return EntryCatalogUpdate::applied;
    if(!services)return EntryCatalogUpdate::unsupported;
    auto* peer=services->find_in_zone(actor,state.request_1754);
    if(peer) {
        auto lease=services->retain(*peer);
        if(!lease)throw std::logic_error("Cannot retain guild invitation peer");
        if(packet[2]==1) {
            if(services->can_invite(*peer)) {
                if(!services->join(actor,*peer))return EntryCatalogUpdate::unsupported;
                net::Packet response{6,0x0d,1};
                for(unsigned i=0;i<4;++i)response.push_back(static_cast<std::uint8_t>(state.request_1754>>(8*i)));
                peer->send_self(response);
            }
            state.request_1754=state.deadline_5838=0;
            return EntryCatalogUpdate::applied;
        }
        net::Packet response{6,0x0d,0};
        for(unsigned i=0;i<4;++i)response.push_back(static_cast<std::uint8_t>(state.request_1754>>(8*i)));
        peer->send_self(response);
    }
    state.request_1754=state.deadline_5838=0;
    return EntryCatalogUpdate::applied;
}
}
