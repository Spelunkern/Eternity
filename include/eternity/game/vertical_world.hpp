#pragma once
#include "eternity/game/mob_primary_zones.hpp"
#include "eternity/game/mob_zone_services.hpp"

namespace eternity::game {
class VerticalMobsWorld;
// The legacy callback owns one primary-zone frame, including its population
// pass. Original cross-zone phase ordering requires visit_primary_zones instead.
// The caller stops all producers before shutdown.
struct VerticalWorldPorts {
    std::function<void(MobPrimaryZones&)> attach_obelisks;
    std::function<void(RuntimeZone&, std::uint32_t)> frame; // Optional for phased callers; frame() rejects an empty provider.
    std::function<std::uint32_t()> reset_tick;
};
class VerticalWorld final {
public:
    static std::unique_ptr<VerticalWorld> create(std::shared_ptr<ServerMaps>,
        MobZoneServicesInputs, std::unique_ptr<MobPool>, MobPrimaryZoneTransport, VerticalWorldPorts);
    static std::unique_ptr<VerticalWorld> create(std::shared_ptr<ServerMaps>,
        std::shared_ptr<ZoneServices>, std::unique_ptr<MobPool>, MobPrimaryZoneTransport, VerticalWorldPorts);
    bool start();
    // Read only on the serialized world executor.
    bool running() const noexcept {return state_==State::running;}
    void frame(std::uint32_t now);
    // Hold the existing authority over a multi-pass update. The callback must
    // execute the real update phases and stop if shutdown is requested.
    void serialized_update(const std::function<void(VerticalWorld&)>&);
    // One serialized phase over the retained primary owners. A full original world update
    // also visits active dynamic zones, which this authority does not own.
    void visit_primary_zones(const std::function<void(RuntimeZone&)>&);
    void visit_primary_zones_indexed(const std::function<void(std::size_t,RuntimeZone&)>&);
    // One actual primary CZone::UpdateMobIdle pass. Providers must be the
    // retained zone's real RNG, mob initialization, AI and linked-mob owners.
    // False means shutdown was requested before the respawn pass completed.
    bool update_primary_mob_idle(std::size_t, std::uint32_t, RandomSource&,
        VerticalMobsWorld&, MobInitPorts&, LinkedMobWorld&);
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
