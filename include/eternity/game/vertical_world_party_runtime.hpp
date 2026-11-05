#pragma once
#include "eternity/game/vertical_world_party_membership.hpp"
#include "eternity/game/vertical_world_party_requests.hpp"
#include "eternity/game/vertical_world_send_buffer_pool.hpp"

namespace eternity::game {
// World-scoped composition of the retained party owners. The session registry,
// world index and all required services outlive this object. Calls, membership
// changes and shutdown are serialized on the common world strand. Bound
// gameplay/lifecycle callbacks and pending buffer references must drain before
// shutdown and must never outlive the runtime.
// ps_game.exe CUser::PacketParty 004676c0 / CUser::LeaveWorld 0044aad0.
class VerticalWorldPartyRuntime final {
public:
    VerticalWorldPartyRuntime(VerticalWorldSession::Strand strand,VerticalWorldSession::Registry& sessions,WorldUsers& users,
        VerticalWorldPartyMembership::Services membership,VerticalWorldPartyRequests::Close close,VerticalWorldPartyRequests::Remaining remaining)
        :strand_(std::move(strand)),directory_(strand_,sessions,users),membership_(directory_,registry_,buffers_,std::move(membership)),
         requests_(directory_,membership_,registry_.registry(),registry_,std::move(close),std::move(remaining)) {requests_.bind_activity_counter(active_);}
    ~VerticalWorldPartyRuntime() {if(inventory_users_!=0)std::terminate();}
    VerticalWorldPartyRuntime(const VerticalWorldPartyRuntime&)=delete;
    VerticalWorldPartyRuntime& operator=(const VerticalWorldPartyRuntime&)=delete;
    VerticalWorldPartyDirectory& directory() {require_active();return directory_;}
    VerticalWorldPartyRegistry& registry() {require_active();return registry_;}
    VerticalWorldSendBufferPool& buffers() {require_active();return buffers_;}
    VerticalWorldPartyMembership& membership() {require_active();return membership_;}
    VerticalWorldPartyRequests& requests() {require_active();return requests_;}
    // Inventory gameplay retains this owner until synchronous dispatch drains.
    void retain_inventory() {require_active();++inventory_users_;}
    void release_inventory() noexcept {
        if(!strand_.running_in_this_thread() || inventory_users_==0)std::terminate();
        --inventory_users_;
    }
    void shutdown() {
        require_active();
        if(inventory_users_!=0)throw std::logic_error("Party runtime still has inventory gameplay");
        if(active_!=0)throw std::logic_error("Party runtime still has active requests");
        if(buffers_.outstanding()!=0)throw std::logic_error("Party runtime still has send buffers");
        registry_.shutdown();stopped_=true;
    }
private:
    void require_active() const {
        if(!strand_.running_in_this_thread())throw std::logic_error("Party runtime outside world strand");
        if(stopped_)throw std::logic_error("Party runtime is stopped");
    }
    VerticalWorldSession::Strand strand_;VerticalWorldSendBufferPool buffers_;VerticalWorldPartyRegistry registry_;
    VerticalWorldPartyDirectory directory_;VerticalWorldPartyMembership membership_;std::size_t active_=0,inventory_users_=0;VerticalWorldPartyRequests requests_;bool stopped_=false;
};
}
