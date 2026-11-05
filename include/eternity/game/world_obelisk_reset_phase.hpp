#pragma once
#include "eternity/game/vertical_world.hpp"
#include <functional>
#include <stdexcept>

namespace eternity::game {
enum class WorldObeliskResetResult {skipped,completed,interrupted};
// The reset portion of CWorldThread::UpdateKillCount 00407c30; the caller
// must first execute both countries' real kill-count apply/send operations.
class WorldObeliskResetPhase final {
public:
    void request() noexcept {requested_=true;}
    bool pending() const noexcept {return requested_;}
    template<class Root> WorldObeliskResetResult update(Root& root,const std::function<std::uint32_t()>& tick) {
        if(!tick) throw std::invalid_argument("Obelisk reset requires the real world clock");
        auto& world=root.world();if(!world.running()) throw std::logic_error("World is not running");
        if(!requested_) return WorldObeliskResetResult::skipped;
        bool complete=true;
        const bool visited=root.visit_active_zones([&](RuntimeZone& zone){
            if(!world.running()) {complete=false;return;}
            if(!zone.map) throw std::logic_error("Obelisk reset zone lost its map");
            if(zone.map->settings.create_type!=0) return;
            auto authority=world.zones();
            if(!authority->reset(authority->map_index(*zone.map),tick)) throw std::logic_error("Obelisk reset lost its retained owner");
            if(!world.running()) complete=false;
        });
        if(!visited || !complete) return WorldObeliskResetResult::interrupted;
        requested_=false;return WorldObeliskResetResult::completed;
    }
private:
    bool requested_=false;
};
}
