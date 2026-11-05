#pragma once
#include "eternity/game/world_primary_zone_packets.hpp"
#include <memory>
#include <stdexcept>

namespace eternity::game {
// CWorldThread::Update 0040747b..004074a3: one packet pass on every active
// zone, in the live thread-list order, on every frame. The actual zone packet
// helper uses retained admission, user, session, clock and movement owners.
class WorldActivePacketPhase final {
public:
    explicit WorldActivePacketPhase(std::shared_ptr<PrimaryZonePacketServices> services):services_(std::move(services)) {
        if(!services_) throw std::invalid_argument("Active packet phase requires real session and movement services");
    }
    template<class Root> bool update(Root& root) {
        auto& world=root.world();
        if(!world.running()) throw std::logic_error("World is not running");
        bool complete=true;
        const bool visited=root.visit_active_zones([&](RuntimeZone& zone){
            if(!world.running() || !update_primary_zone_packets(world,root.admission(),zone,*services_,[&](RuntimeZone& waiting){root.update_zone_wait_packets(waiting);})) complete=false;
        });
        return visited && complete;
    }
private:
    std::shared_ptr<PrimaryZonePacketServices> services_;
};
}
