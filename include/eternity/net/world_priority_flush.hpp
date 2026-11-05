#pragma once
#include "eternity/net/world_priority_batch.hpp"
#include "eternity/game/user_priority_queue.hpp"
#include <algorithm>
#include <memory>
#include <stdexcept>
#include <vector>

namespace eternity::net {
// Owner retains handles, queue, pool, channel and executors through actual
// completion. Call only on the pool's world strand. Callbacks and pool release
// must not throw. active is the owner's synchronous per-connection visit fence.
// Stop/disconnect must separately await all handles after their callbacks return.
inline void flush_world_priority(WorldPriorityBatch::Strand strand,
    game::UserPriorityQueue& queue,std::shared_ptr<game::PartyMembershipSendWorld> pool,
    const WorldChannel& channel,std::vector<std::shared_ptr<WorldPriorityBatch>>& handles,
    bool& active,std::function<void(bool)> completion) {
    if(!strand.running_in_this_thread() || !pool || !queue.owns(*pool) || !completion ||
       !channel.connection_identity || !channel.pending_send_state || !channel.send_batch_tracked)
        throw std::invalid_argument("Priority flush requires its actual serialized owners");
    if(active)throw std::logic_error("Priority flush reentered");
    active=true;
    struct Guard {game::UserPriorityQueue& queue;bool& active;~Guard(){queue.reset();active=false;}} guard{queue,active};
    std::erase_if(handles,[](const auto& handle){return handle && handle->complete();});
    while(queue.size()) {
        // This is a snapshot; the real channel atomically rechecks admission.
        const auto identity=channel.connection_identity();const auto state=channel.pending_send_state();
        if(!identity || !state || state->batch_limit<=0 || state->batches>=static_cast<std::size_t>(state->batch_limit))break;
        handles.reserve(handles.size()+1); // Retain accepted work without later allocation.
        auto batch=queue.take_batch();if(!batch)break;
        try {
            auto handle=WorldPriorityBatch::submit(strand,pool,*batch,channel,completion);
            if(handle) {handles.push_back(std::move(handle));continue;}
        } catch(...) {
            for(auto* buffer:*batch)game::release_party_membership_buffer(*buffer,*pool);
            throw;
        }
        // The original worker's Reset drops unsent queue references. A batch
        // extracted before a close/capacity race remains ours on rejection.
        for(auto* buffer:*batch)game::release_party_membership_buffer(*buffer,*pool);
        break;
    }
}
}
