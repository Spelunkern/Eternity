#pragma once
#include "eternity/game/party_runtime_raid_requests.hpp"
namespace eternity::game {
// Whole PacketParty 0b1a branch, including captured CPartyLock, locked leader
// predicate and FindUser's post-unlock slot reload. False means other opcode.
bool dispatch_raid_expulsion_request(const PartyRequestUser&,net::Packet&,PartyRaidRequestsWorld&);
}
