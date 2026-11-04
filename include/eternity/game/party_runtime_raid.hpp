#pragma once
#include "eternity/game/party_runtime_requests.hpp"
#include "eternity/game/party_runtime_membership_user_send.hpp"
namespace eternity::game {
// The SAME registry object holds an ordered 30-entry array. Index is the wire
// group/slot identity; it is not the array position and holes are not repaired.
PartyMembershipUser*& raid_membership_slot(PartyRegistryParty&,std::int32_t);
std::uint32_t& raid_membership_index(PartyRegistryParty&,std::int32_t);
std::int32_t empty_raid_membership_slot(PartyRegistryParty&); // 00446480
bool enter_raid_party(PartyRegistryParty&,PartyMembershipUser&,PartyMembershipWorld&); // 00445a30
bool leave_raid_party(PartyRegistryParty&,PartyMembershipUser&,PartyMembershipWorld&); // 00446510
std::uint8_t raid_membership_type(PartyMembershipUser&,PartyMembershipWorld&); // 0048a330, 30 slots
// Reuses existing allocation, direct send and reference completion owners.
// Pool/worker/scene/log/guild/registry/locks remain REQUIRED real providers.
class PartyRaidMembershipWorld:public PartyMembershipUserSendWorld {
public:
    void send_party(PartyNormalMembership&,const net::Packet&) override;
    std::uint8_t party_type(PartyMembershipUser&) override;
};
void send_raid_membership(PartyRegistryParty&,std::span<const std::uint8_t>,PartyMembershipSendWorld&);
// Concrete membership routing for PacketParty and its default real ExpelUser.
// membership_world must provide 30-slot classification/send (above or equivalent).
// Full raid request dispatch remains an explicit inherited required provider.
class PartyRaidRequestWorld:public PartyRequestWorld {
public:
    bool enter_raid(PartyRegistryParty&,const PartyRequestUser&) override;
    void leave_raid(PartyRegistryParty&,const PartyRequestUser&) override;
};
}
