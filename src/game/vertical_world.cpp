#include "eternity/game/vertical_world.hpp"
#include "eternity/game/vertical_mobs.hpp"
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
    if(!maps_ || !zones_ || !ports_.attach_obelisks || !ports_.reset_tick)
        throw std::invalid_argument("World requires maps, primary authority and real lifecycle providers");
}
bool VerticalWorld::start() {
    if(state_!=State::fresh) throw std::logic_error("World start already attempted");
    state_=State::starting;
    try {
        if(!zones_->initialize()) {state_=State::closed;return false;}
        initialized_=true;
        // Providers may request shutdown while initialization is on the stack.
        // Never revive the world or start another zone after that request.
        if(state_!=State::starting) { shutdown(); return false; }
        ports_.attach_obelisks(*zones_);
        if(state_!=State::starting) { shutdown(); return false; }
        for(std::size_t i=0;i<maps_->size();++i) {
            if(maps_->all()[i].settings.create_type==0 && !zones_->startup(i))
                throw std::runtime_error("Primary zone startup unavailable");
            if(state_!=State::starting) { shutdown(); return false; }
        }
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
    if(!ports_.frame) throw std::logic_error("Legacy zone frame provider is not bound");
    visit_primary_zones([&](RuntimeZone& zone){ports_.frame(zone,now);});
}
void VerticalWorld::serialized_update(const std::function<void(VerticalWorld&)>& update) {
    if(!update) throw std::invalid_argument("Missing world update");
    if(state_!=State::running) throw std::logic_error("World is not running");
    // CWorldThread::Update 00407390 holds the active zone-list lock across
    // the complete sequence of passes, not only a single zone traversal.
    zones_->serialized([&]{update(*this);});
}
void VerticalWorld::visit_primary_zones(const std::function<void(RuntimeZone&)>& visit) {
    if(!visit) throw std::invalid_argument("Missing primary zone visitor");
    visit_primary_zones_indexed([&](std::size_t,RuntimeZone& zone){visit(zone);});
}
void VerticalWorld::visit_primary_zones_indexed(const std::function<void(std::size_t,RuntimeZone&)>& visit) {
    if(!visit) throw std::invalid_argument("Missing primary zone visitor");
    if(state_!=State::running) throw std::logic_error("World is not running");
    // Keep the authority serialized for the complete pass, as the original
    // world thread holds its zone-list lock across its update traversals.
    zones_->serialized([&] {
        for(std::size_t i=0;i<maps_->size();++i) if(maps_->all()[i].settings.create_type==0) {
            auto lease=zones_->lease(i);
            if(!lease) throw std::runtime_error("Primary zone unavailable during frame");
            lease.visit([&](RuntimeZone& zone){visit(i,zone);});
            // Shutdown remains deferred until this dispatch and its lease exit.
            if(state_!=State::running) return;
        }
    });
}
bool VerticalWorld::update_primary_mob_idle(std::size_t id,std::uint32_t now,RandomSource& random,
    VerticalMobsWorld& idle,MobInitPorts& init,LinkedMobWorld& linked) {
    if(state_!=State::running) throw std::logic_error("World is not running");
    auto lease=zones_->lease(id);
    if(!lease) throw std::runtime_error("Primary zone unavailable during mob idle");
    bool complete=false;
    lease.visit([&](RuntimeZone& zone){
        // ps_game.exe CZone::UpdateMobIdle 00421a00: idle, deaths, then
        // UpdateMobRespawn 0041d240 (regular, obelisk, linked).
        if(!vertical_mobs_idle_frame(*zones_,id,now,random,idle))
            throw std::runtime_error("Primary zone unavailable during mob idle");
        if(state_!=State::running) return;
        if(!zone.map || !zone.mobs || !zone.linked_mobs)
            throw std::logic_error("Primary zone lost its mob owners");
        struct Interrupted {};
        try {
            zone.update_mob_population(zones_->mob_pool(),init,now,
                [&](std::uint32_t tick){
                    if(state_!=State::running) throw Interrupted{};
                    if(!zones_->tick(id,tick)) {
                        if(state_!=State::running) throw Interrupted{};
                        throw std::runtime_error("Primary obelisk update unavailable");
                    }
                },
                [&](std::uint32_t tick){
                    if(state_!=State::running) throw Interrupted{};
                    zone.linked_mobs->update(zone.map->definition,*zone.mobs,
                        zones_->mob_pool(),init,tick,linked);
                });
        } catch(const Interrupted&) {return;}
        complete=state_==State::running;
    });
    return complete;
}
bool VerticalWorld::shutdown() {
    if(state_==State::closed) return true;
    if(state_==State::fresh) {state_=State::closed;return true;}
    state_=State::closing;
    // Initialization may be executing inside a provider; its owner is not
    // drainable until initialize() returns and records that it succeeded.
    if(!initialized_) return false;
    if(initialized_ && !zones_->drain_all(ports_.reset_tick)) return false;
    state_=State::closed;return true;
}
}
