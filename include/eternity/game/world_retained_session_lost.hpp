#pragma once
#include "eternity/game/world_retained_global_send.hpp"
#include "eternity/game/world_session_lost_phase.hpp"

namespace eternity::game {
// Session::channel() is its actual retained socket channel. The connection
// timestamp must come from that socket's accept/connect event, not actor entry.
template<class Session> class WorldRetainedSessionLost final:public WorldSessionLostServices {
public:
    using ConnectedTick=std::function<std::uint32_t(Session&)>;
    WorldRetainedSessionLost(std::shared_ptr<WorldRetainedGlobalSend<Session>> sessions,ConnectedTick tick)
        :sessions_(std::move(sessions)),tick_(std::move(tick)) {
        if(!sessions_ || !tick_)throw std::invalid_argument("Session loss requires actual retained connections and connect-time owner");
    }
    std::shared_ptr<void> retain(WorldAdmissionUser& user) override{return sessions_->retain(user);}
    std::uint32_t connected_tick(WorldAdmissionUser& user) override {
        return sessions_->visit_session(user,[&](Session& session){return tick_(session);});
    }
    void close(WorldAdmissionUser& user,std::uint32_t reason) override {
        if(reason!=8)throw std::invalid_argument("Unexpected session-lost close reason");
        sessions_->visit_session(user,[&](Session& session){auto& channel=session.channel();
            if(!channel.disconnect_with_reason)throw std::logic_error("Session-lost connection has no close owner");
            channel.disconnect_with_reason(reason);
        });
    }
private:
    std::shared_ptr<WorldRetainedGlobalSend<Session>> sessions_;ConnectedTick tick_;
};
}
