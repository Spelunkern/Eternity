#pragma once
#include "eternity/game/party_runtime_raid_requests.hpp"
namespace eternity::game {
bool exchange_raid_wire_slots(PartyRegistryParty&,std::uint32_t,std::uint32_t,PartyMembershipWorld&); // ChangeUser 00445110
bool select_raid_sub_leader(PartyRegistryParty&,std::uint32_t,PartyMembershipWorld&); // 00445220
bool select_raid_leader(PartyRegistryParty&,std::uint32_t,PartyMembershipWorld&); // 004452f0
// Handles whole PacketParty 0b12/0b13/0b17 branches; false means other opcode.
bool dispatch_raid_leadership_request(const PartyRequestUser&,net::Packet&,PartyRaidRequestsWorld&);
}
