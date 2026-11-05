#pragma once
#include "eternity/game/user_priority_queue.hpp"
#include "eternity/game/user_view_world_channel.hpp"
#include <functional>
#include <stdexcept>

namespace eternity::game {
// CZone::UpdateNetSend 00421730, SWorker::SendPostQueuePriority 004dc2c0
// and SSyncQueueBufferPriority::Reset 004dfe60. WorldChannel already posts
// ordinary sends; this transfers priority batches and resets unsent entries.
// can_submit must observe the real connection's pending-send threshold.
inline void flush_world_connection_priority(std::int32_t socket,UserPriorityQueue& queue,
    PartyMembershipSendWorld& pool,const net::WorldChannel& channel,const std::function<bool()>& can_submit) {
    if(!queue.owns(pool) || !can_submit)throw std::invalid_argument("Priority flush requires the actual pool and connection capacity owner");
    if(socket!=-1) {
        while(queue.size()!=0 && can_submit()) {
            auto batch=queue.take_batch();
            if(!batch)break; // Original allocation failure leaves entries for Reset.
            deliver_priority_batch(*batch,pool,channel);
        }
    }
    queue.reset();
}
}
