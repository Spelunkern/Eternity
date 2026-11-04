#pragma once
#include "eternity/game/party_runtime_requests.hpp"
namespace eternity::game {
// ps_game.exe CParty::ExpelUser 00447120. The selected party and every slot
// remain alive through nested Leave and callbacks, including after unregister.
// Unlike LeaveNormalParty, this retains leader, cursor and slot indices.
bool expel_party_request_user(PartyRegistryParty&,const PartyRequestUser&,PartyRequestWorld&);
}
