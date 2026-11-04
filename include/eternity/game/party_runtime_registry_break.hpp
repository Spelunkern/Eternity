#pragma once
#include "eternity/game/party_runtime_registry.hpp"
namespace eternity::game {
// ps_game.exe CParty::BreakParty 00444fa0. Caller supplies <=30 readable live
// slots; callbacks must keep each reloaded subject alive. Unlike normal Leave,
// this emits no 0b05/0b07 and does not recurse or clean guild creation state.
// Reset and unregister happen after user notifications, under the party lock.
bool break_registry_party(PartyRegistry&,PartyRegistryParty&,PartyRegistryPorts&,PartyMembershipWorld&);
}
