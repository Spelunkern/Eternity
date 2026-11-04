#include "eternity/game/vertical_world.hpp"
#include <stdexcept>
#include <utility>
namespace eternity::game {
std::unique_ptr<VerticalWorld> VerticalWorld::create(std::shared_ptr<ServerMaps> maps,
    MobZoneServicesInputs inputs, std::unique_ptr<MobPool> pool, MobPrimaryZoneTransport transport, VerticalWorldPorts ports) {
    if (!pool || &inputs.pool != pool.get())
        throw std::invalid_argument("World requires the same bound mob pool");
    auto services=MobZoneServices::create(std::move(inputs));
    return create(std::move(maps),std::move(services),std::move(pool),std::move(transport),std::move(ports));
}
std::unique_ptr<VerticalWorld> VerticalWorld::create(std::shared_ptr<ServerMaps> maps,
    std::shared_ptr<ZoneServices> services, std::unique_ptr<MobPool> pool, MobPrimaryZoneTransport transport, VerticalWorldPorts ports) {
    auto zones=MobPrimaryZones::create(maps,std::move(services),std::move(pool),std::move(transport));
    return std::unique_ptr<VerticalWorld>(new VerticalWorld(std::move(maps),std::move(zones),std::move(ports)));
}
VerticalWorld::VerticalWorld(std::shared_ptr<ServerMaps> maps,std::shared_ptr<MobPrimaryZones> zones,VerticalWorldPorts ports)
    :maps_(std::move(maps)),zones_(std::move(zones)),ports_(std::move(ports)) {
    if(!maps_ || !zones_ || !ports_.attach_obelisks || !ports_.frame || !ports_.reset_tick)
        throw std::invalid_argument("World requires maps, primary authority and real lifecycle providers");
}
bool VerticalWorld::start() {
    if(state_!=State::fresh) throw std::logic_error("World start already attempted");
    state_=State::starting;
    try {
        if(!zones_->initialize()) {state_=State::closed;return false;}
        initialized_=true;
        ports_.attach_obelisks(*zones_);
        for(std::size_t i=0;i<maps_->size();++i)
            if(maps_->all()[i].settings.create_type==0 && !zones_->startup(i))
                throw std::runtime_error("Primary zone startup unavailable");
        state_=State::running;return true;
    } catch(...) {
        state_=State::closing;
        // Keep this object and its authority alive if cleanup is deferred or throws.
        // The caller must retry shutdown after stopping providers and releasing leases.
        if(initialized_) { try { if(zones_->drain_all(ports_.reset_tick)) state_=State::closed; } catch(...) {} }
        else state_=State::closed;
        throw;
    }
}
void VerticalWorld::frame(std::uint32_t now) {
    if(state_!=State::running) throw std::logic_error("World is not running");
    for(std::size_t i=0;i<maps_->size();++i) if(maps_->all()[i].settings.create_type==0) {
        auto lease=zones_->lease(i);
        if(!lease) throw std::runtime_error("Primary zone unavailable during frame");
        lease.visit([&](RuntimeZone& zone){ports_.frame(zone,now);});
    }
}
bool VerticalWorld::shutdown() {
    if(state_==State::closed) return true;
    if(state_==State::fresh) {state_=State::closed;return true;}
    state_=State::closing;
    if(initialized_ && !zones_->drain_all(ports_.reset_tick)) return false;
    state_=State::closed;return true;
}
}
