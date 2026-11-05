#pragma once
#include "eternity/game/vertical_world.hpp"
#include <stdexcept>

namespace eternity::game {
struct PrimaryWeatherBindings {RandomSource& random;ZoneEnvironmentSink& sink;};
class PrimaryWeatherServices {
public:
    virtual ~PrimaryWeatherServices()=default;
    // Must bind the executor's real RNG and this exact zone's broadcast sink.
    virtual PrimaryWeatherBindings bindings(RuntimeZone&)=0;
};
enum class PrimaryWeatherResult {skipped,completed,interrupted};
// CWorldThread::Update 00407765..00407885. The active alternative includes
// retained instance zones and shares the primary alternative's deadline.
class WorldPrimaryWeatherPhase final {
public:
    explicit WorldPrimaryWeatherPhase(std::shared_ptr<PrimaryWeatherServices> services):services_(std::move(services)) {
        if(!services_) throw std::invalid_argument("Primary weather phase requires real RNG and zone transports");
    }
    PrimaryWeatherResult update(VerticalWorld& world,std::uint32_t now) {
        if(!world.running()) throw std::logic_error("World is not running");
        if(now<next_) return PrimaryWeatherResult::skipped;
        auto result=PrimaryWeatherResult::completed;
        world.serialized_update([&](VerticalWorld& current){
            current.visit_primary_zones([&](RuntimeZone& zone){
                if(!dispatch(current,zone,now)) result=PrimaryWeatherResult::interrupted;
            });
        });
        if(result==PrimaryWeatherResult::completed) next_=now+60000U;
        return result;
    }
    template<class Root> PrimaryWeatherResult update_active(Root& root,std::uint32_t now) {
        auto& world=root.world();
        if(!world.running()) throw std::logic_error("World is not running");
        if(now<next_) return PrimaryWeatherResult::skipped;
        bool complete=true;
        const bool visited=root.visit_active_zones([&](RuntimeZone& zone){
            if(!dispatch(world,zone,now)) complete=false;
        });
        if(!visited || !complete) return PrimaryWeatherResult::interrupted;
        next_=now+60000U;return PrimaryWeatherResult::completed;
    }
    std::uint32_t next_deadline() const noexcept {return next_;}
private:
    bool dispatch(VerticalWorld& world,RuntimeZone& zone,std::uint32_t now) {
        if(!world.running()) return false;
        if(!zone.environment) throw std::logic_error("Active zone has no retained environment owner");
        auto bindings=services_->bindings(zone);
        if(!world.running()) return false;
        zone.environment->update_weather(now,bindings.random,bindings.sink);
        return world.running();
    }
    std::shared_ptr<PrimaryWeatherServices> services_;std::uint32_t next_=0;
};
}
