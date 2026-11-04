#pragma once
#include "eternity/game/party_runtime_membership.hpp"

namespace eternity::game {
// ps_game.exe CUser::GetPartyType 0048a330. Return 0 without membership,
// 2 for the live selected leader when count > 0, otherwise 1. The original
// retains the first party lock and acquires the reread party a second time,
// even when both identities are equal. Lock callbacks may change membership;
// every captured party must remain alive until its matching unlock.
std::uint8_t party_membership_type(PartyMembershipUser&,PartyMembershipWorld&);
}
