#pragma once
#include "eternity/game/vertical_world.hpp"
#include <stdexcept>

namespace eternity::game {
enum class PrimaryItemPhaseResult {skipped,completed,interrupted};
// ps_game.exe CWorldThread::Update 004076d4..00407718. This is the primary
// portion only: active instance zones and their original ordering remain the
// world-thread owner's responsibility. Invoke on the serialized world strand.
class WorldPrimaryItemPhase final {
public:
    PrimaryItemPhaseResult update(VerticalWorld& world,std::uint32_t now) {
        if(!world.running()) throw std::logic_error("World is not running");
        if(now<next_) return PrimaryItemPhaseResult::skipped;
        auto result=PrimaryItemPhaseResult::completed;
        world.serialized_update([&](VerticalWorld& current){
            current.visit_primary_zones([&](RuntimeZone& zone){
                if(!zone.items) throw std::logic_error("Primary zone has no retained item owner");
                zone.items->update(now); // CZone::UpdateItem 00421e20.
                if(!current.running()) result=PrimaryItemPhaseResult::interrupted;
            });
        });
        if(result==PrimaryItemPhaseResult::completed) next_=now+10000U;
        return result;
    }
    // Active-list alternative sharing the same 10000 ms deadline.
    template<class Root> PrimaryItemPhaseResult update_active(Root& root,std::uint32_t now) {
        auto& world=root.world();
        if(!world.running()) throw std::logic_error("World is not running");
        if(now<next_) return PrimaryItemPhaseResult::skipped;
        bool complete=true;
        const bool visited=root.visit_active_zones([&](RuntimeZone& zone){
            if(!world.running()) {complete=false;return;}
            if(!zone.items) throw std::logic_error("Active zone has no retained item owner");
            zone.items->update(now);
            if(!world.running()) complete=false;
        });
        if(!visited || !complete) return PrimaryItemPhaseResult::interrupted;
        next_=now+10000U;return PrimaryItemPhaseResult::completed;
    }
    std::uint32_t next_deadline() const noexcept {return next_;}
private:
    std::uint32_t next_=0; // CWorldThread::CWorldThread 00407100.
};
}
