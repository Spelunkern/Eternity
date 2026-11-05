#pragma once
#include "eternity/game/world_retained_session_lost.hpp"
#include "eternity/game/world_send_retained_update.hpp"
#include "eternity/game/world_frame_driver.hpp"

namespace eternity::game {
// The root, registry and real connection resources must share one world strand.
// Production supplies the pre-account owner, the actual immediate and priority
// operations, the socket connect tick, remaining gameplay services and cadence.
template<class Root,class Session> class WorldSendComposition final {
public:
    using Global=WorldRetainedGlobalSend<Session>;
    using Lost=WorldRetainedSessionLost<Session>;
    struct Inputs {
        typename Global::RetainUnreserved retain_unreserved;
        typename Global::Flush send_now,send_priority_and_reset;
        typename Lost::ConnectedTick connected_tick;
        std::shared_ptr<WorldSendRemainingServices> remaining;
        std::function<std::uint32_t()> tick;
        std::uint32_t period_ms=0;
        WorldFrameDriver::OverrunCounter overruns;
        WorldFrameDriver::Failure failure;
    };
    // Call once on the root strand before startup. If a root binding throws
    // after another binding succeeds, discard that unstarted root.
    static void bind(const std::shared_ptr<Root>& root,Inputs inputs) {
        if(!root || !inputs.retain_unreserved || !inputs.send_now || !inputs.send_priority_and_reset ||
            !inputs.connected_tick || !inputs.remaining || !inputs.tick || !inputs.period_ms ||
            inputs.period_ms>0x7fffffffU || !inputs.overruns || !inputs.failure)
            throw std::invalid_argument("World send composition requires all real owners and cadence");
        std::weak_ptr<Root> weak=root;
        auto global=std::make_shared<Global>(root->sessions(),std::move(inputs.retain_unreserved),
            std::move(inputs.send_now),std::move(inputs.send_priority_and_reset),[weak]{
                auto current=weak.lock();return current && current->world_send_running();
            });
        auto lost=std::make_shared<Lost>(global,std::move(inputs.connected_tick));
        auto other=std::make_shared<WorldSendRetainedOther<Root>>(root,std::move(inputs.remaining));
        auto update=std::make_shared<WorldSendRetainedUpdate<Root>>(root,global,other);
        root->bind_session_lost_phase(std::move(lost));
        root->bind_world_send_update(std::move(update),std::move(inputs.tick));
        root->bind_world_send_driver(inputs.period_ms,std::move(inputs.overruns),std::move(inputs.failure));
    }
};
}
