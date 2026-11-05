#pragma once
#include "eternity/game/party_runtime_membership_send.hpp"
#include "eternity/net/world_server.hpp"
#include <stdexcept>

namespace eternity::game {
// Enqueue provider for one actual WorldChannel. The caller already transferred
// one buffer reference to this operation. WorldChannel takes a plaintext copy
// and serializes encryption/framing on the socket executor, so the pool reference
// may be released once that call returns. The void send port may silently
// reject a closed or over-budget connection; return is not acceptance. If copying/posting throws,
// ownership stays with the caller, which must release it. The pool owner outlives
// this call; actual socket close/overflow remains WorldChannel's responsibility.
inline void enqueue_user_view_world_channel(PartyMembershipSendBuffer& buffer,
    PartyMembershipSendWorld& pool,const net::WorldChannel& channel) {
    if(!channel.send || buffer.frame.size()<2 || buffer.frame.size()>2048)
        throw std::invalid_argument("Invalid shared view frame or channel");
    const auto declared=static_cast<std::size_t>(buffer.frame[0])|(static_cast<std::size_t>(buffer.frame[1])<<8);
    if(declared!=buffer.frame.size())throw std::invalid_argument("Shared view frame length mismatch");
    net::Packet payload(buffer.frame.begin()+2,buffer.frame.end());
    channel.send(std::move(payload));
    release_party_membership_buffer(buffer,pool);
}
}

namespace eternity::game {
// Consumes transferred priority-queue references. WorldChannel receives copies
// one packet at a time; unlike the original WSASend batch, socket timing and
// batching remain the channel owner's responsibility. A failed post releases
// the failed and remaining references and propagates the failure.
inline void deliver_priority_batch(std::vector<PartyMembershipSendBuffer*>& batch,
    PartyMembershipSendWorld& pool,const net::WorldChannel& channel) {
    std::size_t index=0;
    try {
        for(;index<batch.size();++index)enqueue_user_view_world_channel(*batch[index],pool,channel);
    } catch(...) {
        for(;index<batch.size();++index)release_party_membership_buffer(*batch[index],pool);
        batch.clear();throw;
    }
    batch.clear();
}
}
