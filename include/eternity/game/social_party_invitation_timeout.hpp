#pragma once
#include "eternity/game/party_runtime_requests.hpp"

namespace eternity::game {
// CUser::UpdateTimeover 0048e39c..0048e3be. The actual retained party request
// owner is shared with invitation handling and zone leave.
inline bool social_party_invitation_timeout(PartyRequestState& state,std::uint32_t tick) noexcept {
    if(!state.invitation_deadline || tick<state.invitation_deadline || !state.inviter_id)return false;
    state.inviter_id=state.invitation_deadline=0;return true;
}
}
