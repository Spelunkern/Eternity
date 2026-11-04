#pragma once
#include "eternity/game/party_runtime_raid_requests.hpp"
namespace eternity::game {
// IsPartySubBoth 00444ed0: nonzero kind guard precedes lock; result captured
// under lock before callbacks on unlock. Uses SAME authoritative member record.
bool is_raid_request_sub_leader(PartyRegistryParty&,PartyMembershipUser&,PartyMembershipWorld&);
// Whole PacketParty 0b14/0b15/0b18; false means another opcode. Caller validates
// decoded sizes 6/3/23. All lookup bindings retain their live records.
bool dispatch_raid_invitation_request(const PartyRequestUser&,net::Packet&,PartyRaidRequestsWorld&);
}
