#pragma once
#include "eternity/game/vertical_session_registry.hpp"
#include "eternity/game/user_priority_queue.hpp"
#include "eternity/game/user_view_world_channel.hpp"

namespace eternity::game {
// Called by the serialized connection/world owner while the registry, pool,
// queued buffers and channel remain alive. One call sends at most the original
// 128-buffer extraction batch; the scheduler must call again for remaining work.
// A null batch means empty queue or allocation failure, never a claim of delivery.
template<class Session> class VerticalMovePriorityChannel final {
public:
    using Queue=std::function<UserPriorityQueue&(Session&)>;
    using Channel=std::function<net::WorldChannel&(Session&)>;
    using Connection=std::function<PartyMembershipUser&(Session&)>;
    VerticalMovePriorityChannel(VerticalSessionRegistry<Session>& registry,PartyMembershipSendWorld& pool,
        Queue queue,Channel channel,Connection connection={}):registry_(registry),pool_(pool),queue_(std::move(queue)),channel_(std::move(channel)),connection_(std::move(connection)) {
        if(!queue_ || !channel_)throw std::invalid_argument("Priority drain requires queue and channel accessors");
    }
    // Returns packets transferred to the channel; delivery or socket write is
    // asynchronous. Exceptions release all extracted references and propagate.
    std::size_t drain(Actor& actor) {
        auto owner=registry_.retain_actor(actor);
        if(!owner)throw std::logic_error("Priority recipient has no retained session");
        auto& queue=queue_(*owner);
        if(!queue.owns(pool_))throw std::logic_error("Priority queue uses a different pool");
        auto& channel=channel_(*owner);
        if(!channel.send)throw std::logic_error("Priority recipient has no channel");
        auto batch=queue.take_batch();if(!batch)return 0;
        const auto count=batch->size();deliver_priority_batch(*batch,pool_,channel);return count;
    }
    // SSyncQueueBufferPriority::Reset 004dfe60: releases every reference
    // still owned by this queue. It does not cancel already extracted sends.
    // Call only on the serialized owner after the applicable send attempt and
    // while the pool is alive; producers must be stopped for final teardown.
    std::size_t reset_pending(Actor& actor) {
        auto owner=registry_.retain_actor(actor);
        if(!owner)throw std::logic_error("Priority recipient has no retained session");
        auto& queue=queue_(*owner);
        if(!queue.owns(pool_))throw std::logic_error("Priority queue uses a different pool");
        std::size_t count=0;
        while(auto* buffer=queue.take_first()){++count;release_party_membership_buffer(*buffer,pool_);}
        return count;
    }
    // Final close cleanup: require the retained session's actual connection
    // projection to be disconnected before releasing pending references.
    // This cannot cancel extracted batches or prove socket writes have drained.
    std::size_t reset_closed(Actor& actor) {
        auto owner=registry_.retain_actor(actor);
        if(!owner || !connection_)throw std::logic_error("Closed priority reset requires a retained connection");
        const auto& connection=connection_(*owner);
        if(connection.id!=actor.id() || connection.send_socket!=-1)throw std::logic_error("Priority connection is not closed");
        return reset_pending(actor);
    }
private:
    VerticalSessionRegistry<Session>& registry_;PartyMembershipSendWorld& pool_;Queue queue_;Channel channel_;Connection connection_;
};
}
