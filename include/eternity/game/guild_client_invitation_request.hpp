#pragma once
#include "eternity/game/entry_catalogs.hpp"
#include "eternity/game/guild_membership.hpp"
#include "eternity/game/zone_leave_requests.hpp"
#include <span>
#include <array>

namespace eternity::game {
class GuildInvitationRequestServices:public ZoneLeaveRequestServices {
public:
    virtual bool target_blocked(Actor&)=0;
};
// 00476ca2..00476cd7. The original sends two bytes of pre-existing stack
// scratch as the header; the caller must supply observed scratch, and must
// retain the eligible target and its request state throughout the send.
inline void guild_client_invitation_request_send(Actor& requester,Actor& target,
    ZoneLeaveRequestState& target_requests,std::array<std::uint8_t,2> scratch) {
    target_requests.request_1754=requester.id();
    target_requests.deadline_5838=requester.tick_ms()+0x7530u;
    net::Packet notice{scratch[0],scratch[1]};
    const auto id=requester.id();for(unsigned i=0;i<4;++i)notice.push_back(static_cast<std::uint8_t>(id>>(8*i)));
    target.send_self(notice);
}
// ps_game.exe CUser::PacketGuild 00476c20..00476c6a and 00476cfb.
// Eligible targets continue to the guild invitation owner.
inline EntryCatalogUpdate guild_client_invitation_request(GuildUser& user,Actor& actor,
    std::span<const std::uint8_t> packet,GuildInvitationRequestServices* services,
    const std::function<void(net::Packet)>& send) {
    if(packet.size()<6 || packet[0]!=5 || packet[1]!=13)return EntryCatalogUpdate::rejected;
    std::uint32_t id=0;for(unsigned i=0;i<4;++i)id|=std::uint32_t(packet[2+i])<<(8*i);
    auto reject=[&] {if(!send)throw std::logic_error("Missing guild invitation client transport");
        net::Packet response{6,13,0};for(unsigned i=0;i<4;++i)response.push_back(static_cast<std::uint8_t>(id>>(8*i)));
        send(std::move(response));return EntryCatalogUpdate::applied;};
    if(user.rank>2)return reject();
    if(!services)return EntryCatalogUpdate::unsupported;
    auto* peer=services->find_in_zone(actor,id);if(!peer)return reject();
    auto lease=services->retain(*peer);if(!lease)throw std::logic_error("Cannot retain guild invitation target");
    if(peer->id()==actor.id() || peer->appearance().identity.country!=actor.appearance().identity.country)return reject();
    if(services->target_blocked(*peer))return reject();
    return EntryCatalogUpdate::unsupported;
}
}
