#pragma once
#include "eternity/game/user_priority_queue.hpp"
#include "eternity/game/user_priority_view.hpp"
#include "eternity/game/vertical_move_interactions.hpp"
#include "eternity/game/vertical_move_guild_duel.hpp"
#include "eternity/game/zone_leave_interactions.hpp"

namespace eternity::game {
// One instance belongs to one retained session. The connection and shop are
// canonical projections shared with party/shop owners; the pool outlives this
// object and all extracted sends. All access is on the world executor. A
// reference borrowed from an accessor must not survive close_queue(). A retained
// resource handle may outlive the session, but its accessors then fail closed.
class VerticalMoveSessionResources final {
public:
    VerticalMoveSessionResources(PartyMembershipSendWorld& pool,PartyMembershipUser& connection,ZoneLeaveShopState& shop)
        :pool_(pool),connection_(connection),shop_(shop),queue_(pool) {}
    VerticalMoveSessionResources(const VerticalMoveSessionResources&)=delete;
    VerticalMoveSessionResources& operator=(const VerticalMoveSessionResources&)=delete;
    PartyMembershipSendWorld& pool() {require_open();return pool_;}
    UserPriorityQueue& queue() {require_open();return queue_;}
    PartyMembershipUser& connection() {require_open();return connection_;}
    ZoneLeaveShopState& shop() {require_open();return shop_;}
    UserPriorityViewState& cadence() {require_open();return cadence_;}
    VerticalNpcInteraction& npc() {require_open();return npc_;}
    VerticalDuelMovement& duel() {require_open();return duel_;}
    VerticalGuildDuelMovement& guild_duel() {require_open();return guild_duel_;}
    // Invoke only after packet producers are stopped and the real connection
    // projection is closed. Extracted batches and IO leases have separate fences.
    std::size_t close_queue() {
        require_open();
        if(connection_.send_socket!=-1)throw std::logic_error("Movement connection is still open");
        std::size_t count=0;
        while(auto* buffer=queue_.take_first()){++count;release_party_membership_buffer(*buffer,pool_);}
        closed_=true;return count;
    }
private:
    void require_open() const {if(closed_)throw std::logic_error("Movement resources are closed");}
    PartyMembershipSendWorld& pool_;PartyMembershipUser& connection_;ZoneLeaveShopState& shop_;
    UserPriorityQueue queue_;UserPriorityViewState cadence_;VerticalNpcInteraction npc_;
    VerticalDuelMovement duel_;VerticalGuildDuelMovement guild_duel_;bool closed_=false;
};
}
