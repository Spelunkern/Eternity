#pragma once
#include "eternity/game/world_admission.hpp"
#include <memory>
#include <stdexcept>

namespace eternity::game {
class WorldSessionLostServices {
public:
    virtual ~WorldSessionLostServices()=default;
    virtual std::shared_ptr<void> retain(WorldAdmissionUser&)=0;
    // SConnectionParam::m_dwConnectTime: SConnection+0x40.
    virtual std::uint32_t connected_tick(WorldAdmissionUser&)=0;
    virtual void close(WorldAdmissionUser&,std::uint32_t reason)=0;
};
// ps_game.exe CWorld::UpdateSessionLost 0040c2b0, disassembly 0040c2e0..0040c2f0.
class WorldSessionLostPhase final {
public:
    explicit WorldSessionLostPhase(std::shared_ptr<WorldSessionLostServices> services):services_(std::move(services)) {
        if(!services_)throw std::invalid_argument("Session-lost phase requires actual connection services");
    }
    template<class Root> bool update(Root& root,std::uint32_t now) {
        if(!root.world_send_running())throw std::logic_error("Session-lost update requires a running world");
        return root.admission().visit_connected_users_while([&](WorldAdmissionUser& user){return services_->retain(user);},
            [&](WorldAdmissionUser& user){const auto tick=services_->connected_tick(user);if(root.world_send_running() && tick+30000U<now)services_->close(user,8);},
            [&]{return root.world_send_running();});
    }
private:std::shared_ptr<WorldSessionLostServices> services_;
};
}
