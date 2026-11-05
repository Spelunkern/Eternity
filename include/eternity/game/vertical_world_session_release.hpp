#pragma once
#include "eternity/game/vertical_world_party_runtime.hpp"
#include "eternity/game/vertical_world_session_runtime.hpp"

namespace eternity::game {
// Compose real party, guild and retained social owners into the original
// LeaveWorld steps. All owners and the returned callbacks remain alive until
// the synchronous leave returns; the caller supplies actual remaining ports
// and an otherwise complete live state. Call on the shared world strand
// immediately before leave_world, without an intervening party mutation.
// ps_game.exe CUser::LeaveWorld 0044aad0 / ReleaseFriend 004757a0.
template<class Resource,class Store,class Social>
WorldLifecyclePorts bind_vertical_world_session_release(VerticalWorldSession& session,
    VerticalWorldSessionRuntime<Resource,Store>& runtime,VerticalWorldPartyRuntime& party,
    std::shared_ptr<Social> social,WorldLifecycleState& state,WorldLifecyclePorts ports) {
    auto& requests=party.requests();const auto kind=requests.lifecycle_party(session);
    auto party_ports=requests.bind_world_release(session,std::move(ports));
    auto result=runtime.bind_social_world_release(session,std::move(social),std::move(party_ports));
    state.party=kind;return result;
}
}
