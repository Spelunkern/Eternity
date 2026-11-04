#pragma once
#include "eternity/game/party_runtime_raid_requests.hpp"
namespace eternity::game {
// SendRaidPartyError 00467690: 0b19 + reason DWORD + UID DWORD.
void send_raid_request_error(const PartyRequestUser&,std::uint32_t,std::uint32_t,PartyMembershipWorld&);
// Whole PacketParty 0b11; false means another opcode. Requires retained active
// name-index lookup distinct from FindUserZoneMove. Decoded request is 23 bytes.
bool dispatch_raid_autojoin_request(const PartyRequestUser&,net::Packet&,PartyRaidRequestsWorld&);
}
