#pragma once
#include "eternity/game/vertical_world.hpp"
#include <cstdint>
#include <stdexcept>

namespace eternity::game {
enum class InstanceCleanupResult {skipped,completed,deferred,interrupted};
// CWorldThread::Update 00407a4c..00407bfa. The caller invokes this after
// world send processing, under the same world strand as the active-zone list.
// Original active list includes primaries; only dynamic zones are retired.
class WorldInstanceCleanupPhase final {
public:
    template<class Root> InstanceCleanupResult update(Root& root,std::uint32_t now) {
        auto& world=root.world();
        if(!world.running()) throw std::logic_error("World is not running");
        auto result=InstanceCleanupResult::completed;
        world.serialized_update([&](VerticalWorld& current){
            auto authority=current.zones();
            // A failed object release has already removed the list node. Retry
            // before traversing, even if the normal unsigned gate is not due.
            if(authority->has_retiring_instance()) {
                const auto retry=root.retry_instance_retirement(now);
                if(!current.running()) {result=InstanceCleanupResult::interrupted;return;}
                if(retry==MobInstanceExpiry::deferred) {result=InstanceCleanupResult::deferred;return;}
                if(retry!=MobInstanceExpiry::retired) throw std::logic_error("Retirement retry did not finish");
            }
            if(now<next_) {result=InstanceCleanupResult::skipped;return;}
            root.thread_zones().visit_active([&](RuntimeZone& zone){
                if(result==InstanceCleanupResult::interrupted) return;
                if(!current.running()) {result=InstanceCleanupResult::interrupted;return;}
                if(!zone.map) throw std::logic_error("Active world zone lost its map");
                if(zone.map->settings.create_type==0) return;
                const auto expired=root.expire_instance(zone,now);
                if(!current.running()) result=InstanceCleanupResult::interrupted;
                else if(expired==MobInstanceExpiry::deferred) result=InstanceCleanupResult::deferred;
            });
        });
        // Native lease deferral leaves the pass due so it can resume promptly.
        if(result==InstanceCleanupResult::completed) next_=now+1000U;
        return result;
    }
    std::uint32_t next_deadline()const noexcept{return next_;}
private:
    std::uint32_t next_=0; // CWorldThread::CWorldThread 00407100.
};
}
