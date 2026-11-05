#pragma once
#include "eternity/game/party_runtime_requests.hpp"
#include "eternity/game/entry_catalogs.hpp"
#include <array>

namespace eternity::game {
// A pinned view of an existing party member and the exact CUser+188 name
// buffer. Bytes after NUL are significant: PacketComm copies all 21 bytes.
struct SocialPartyFindCandidate {
    PartyRequestUser user;
    std::array<std::uint8_t,21> name_bytes;
};
struct SocialPartyFindPorts {
    // Must visit the caller's live CZone user map in ascending entity-ID order,
    // under its real recursive lock, retaining each user through the visitor.
    // Stop immediately when the visitor returns true. The visit does not send.
    std::function<void(PartyRequestUser&,const std::function<bool(const SocialPartyFindCandidate&)>&)> visit_zone;
    // Actual retained connection transport. May synchronously reenter the world.
    std::function<void(PartyRequestUser&,net::Packet)> send;
};
// ps_game.exe CUser::PacketComm 00475870, 220c at 004760ed..00476250.
// The existing PartyMembershipUser::pending_request is CUser+1744; party is
// CUser+173c. The caller and its zone survive both the ack and the locked visit.
inline EntryCatalogUpdate social_party_find(PartyRequestUser owner,std::span<const std::uint8_t> packet,const SocialPartyFindPorts& ports) {
    if(packet.size()<2)return EntryCatalogUpdate::rejected;
    if(packet[0]!=0x0c || packet[1]!=0x22)return EntryCatalogUpdate::unsupported;
    if(!owner.lifetime || !ports.visit_zone || !ports.send)throw std::invalid_argument("Missing party-find owners");
    if(!owner.member.party)owner.member.pending_request=1;
    ports.send(owner,net::Packet{0x0c,0x22,1});
    net::Packet result{0x0d,0x22,0};std::optional<std::uint32_t> previous;bool stopped=false;
    ports.visit_zone(owner,[&](const SocialPartyFindCandidate& candidate) {
        const auto& peer=candidate.user;
        if(stopped || !peer.lifetime || peer.state.zone!=owner.state.zone || (previous && peer.member.id<=*previous))
            throw std::logic_error("Invalid party-find zone iteration");
        previous=peer.member.id;
        const auto level=static_cast<int>(owner.member.level),other=static_cast<int>(peer.member.level);
        if(peer.member.pending_request && peer.state.country==owner.state.country && &peer.member!=&owner.member && other<=level+5 && other>=level-5) {
            result.push_back(static_cast<std::uint8_t>(peer.member.level));result.push_back(peer.member.job);
            result.insert(result.end(),candidate.name_bytes.begin(),candidate.name_bytes.end());
            ++result[2];if(result[2]==30)stopped=true;
        }
        return stopped;
    });
    if(result[2])ports.send(owner,std::move(result));
    return EntryCatalogUpdate::applied;
}
}
