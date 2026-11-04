#pragma once
#include "eternity/game/mob_primary_zones.hpp"
#include "eternity/game/mob_zone_services.hpp"

namespace eternity::game {
// The callback owns the entire primary-zone frame, including the one population
// pass. Invoke it once per zone per scheduler frame under the existing lease.
// The caller stops all producers before shutdown.
struct VerticalWorldPorts {
    std::function<void(MobPrimaryZones&)> attach_obelisks;
    std::function<void(RuntimeZone&, std::uint32_t)> frame;
    std::function<std::uint32_t()> reset_tick;
};
class VerticalWorld final {
public:
    static std::unique_ptr<VerticalWorld> create(std::shared_ptr<ServerMaps>,
        MobZoneServicesInputs, std::unique_ptr<MobPool>, MobPrimaryZoneTransport, VerticalWorldPorts);
    static std::unique_ptr<VerticalWorld> create(std::shared_ptr<ServerMaps>,
        std::shared_ptr<ZoneServices>, std::unique_ptr<MobPool>, MobPrimaryZoneTransport, VerticalWorldPorts);
    bool start();
    void frame(std::uint32_t now);
    // False means outstanding leases/dispatch; release them and retry. Exceptions
    // from real backend cleanup propagate; retry resumes the underlying drain.
    bool shutdown();
    std::shared_ptr<MobPrimaryZones> zones() const noexcept { return zones_; }
private:
    VerticalWorld(std::shared_ptr<ServerMaps>, std::shared_ptr<MobPrimaryZones>, VerticalWorldPorts);
    enum class State { fresh, starting, running, closing, closed };
    std::shared_ptr<ServerMaps> maps_;
    std::shared_ptr<MobPrimaryZones> zones_;
    VerticalWorldPorts ports_;
    State state_=State::fresh;
    bool initialized_=false;
};
}
