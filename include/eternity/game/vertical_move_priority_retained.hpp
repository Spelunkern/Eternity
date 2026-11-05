#pragma once
#include "eternity/game/vertical_session_registry.hpp"
#include "eternity/game/user_priority_shared_cell.hpp"
#include "eternity/game/user_priority_zone_view.hpp"

namespace eternity::game {
// World executor owns the registry, source, zone, pool and per-session queues.
// Accessors select the actual connection projection and queue for each pinned
// session. All sends, queue drains and registry mutations must be serialized.
template<class Session> class VerticalMovePriorityRetained final:public UserPrioritySharedCellServices {
public:
    using Connection=std::function<PartyMembershipUser&(Session&)>;
    using Queue=std::function<UserPriorityQueue&(Session&)>;
    VerticalMovePriorityRetained(VerticalSessionRegistry<Session>& registry,Actor& source,ZoneUsers& zone,
        std::int32_t width,std::int32_t height,UserViewSharedBuffers& buffers,Connection connection,Queue queue)
        :UserPrioritySharedCellServices(buffers),registry_(registry),source_(source),zone_(zone),
         connection_(std::move(connection)),queue_(std::move(queue)),view_(zone,width,height,*this) {
        if(!connection_ || !queue_)throw std::invalid_argument("Priority delivery requires connection and queue accessors");
    }
    std::shared_ptr<void> retain(Actor& actor) override {return pin(actor);}
    UserPosition origin() override {
        auto owner=pin(source_);const auto position=zone_.position(source_.id());
        if(zone_.find_actor(source_.id())!=&source_ || !position)throw std::logic_error("Priority source left its zone");
        return *position;
    }
    PartyMembershipUser& connection(Actor& actor) override {auto owner=pin(actor);return connection_(*owner);}
    UserPriorityQueue& connection_queue(Actor& actor) override {auto owner=pin(actor);return queue_(*owner);}
    UserPriorityZoneView& view() noexcept {return view_;}
    VerticalMovePriorityRetained(const VerticalMovePriorityRetained&)=delete;
    VerticalMovePriorityRetained& operator=(const VerticalMovePriorityRetained&)=delete;
private:
    std::shared_ptr<Session> pin(Actor& actor) {
        auto result=registry_.retain_actor(actor);if(!result)throw std::logic_error("Priority actor has no retained session");return result;
    }
    VerticalSessionRegistry<Session>& registry_;Actor& source_;ZoneUsers& zone_;Connection connection_;Queue queue_;UserPriorityZoneView view_;
};
}
