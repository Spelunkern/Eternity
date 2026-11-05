#pragma once
#include "eternity/game/vertical_world.hpp"
#include "eternity/game/npc_patrol.hpp"

namespace eternity::game {
struct PrimaryNpcIdleBindings {NpcPool& pool;RandomSource& random;};
class PrimaryNpcIdleServices {
public:
    virtual ~PrimaryNpcIdleServices()=default;
    virtual PrimaryNpcIdleBindings bindings(RuntimeZone&)=0;
    // Must invoke the retained guard AI and its real combat/target services.
    virtual void update_guard(RuntimeZone&,Npc&,std::uint32_t)=0;
};
enum class PrimaryNpcIdleResult {skipped,completed,interrupted};
// CWorldThread::Update 0040767d..004076cf / CZone::UpdateNpcIdle 00422400.
// Primary zones only; active instance zones require their actual world owner.
class WorldPrimaryNpcIdlePhase final {
public:
    explicit WorldPrimaryNpcIdlePhase(std::shared_ptr<PrimaryNpcIdleServices> services):services_(std::move(services)) {
        if(!services_) throw std::invalid_argument("Primary NPC idle phase requires real NPC and guard owners");
    }
    PrimaryNpcIdleResult update(VerticalWorld& world,std::uint32_t now) {
        if(!world.running()) throw std::logic_error("World is not running");
        if(now<next_) return PrimaryNpcIdleResult::skipped;
        auto result=PrimaryNpcIdleResult::completed;
        world.serialized_update([&](VerticalWorld& current){
            current.visit_primary_zones([&](RuntimeZone& zone){
                if(!dispatch_idle(current,zone,now)) result=PrimaryNpcIdleResult::interrupted;
            });
        });
        if(result==PrimaryNpcIdleResult::completed) next_=now+3000U;
        return result;
    }
    // Active-list alternative sharing this same 3000 ms deadline/provider.
    template<class Root> PrimaryNpcIdleResult update_active(Root& root,std::uint32_t now) {
        auto& world=root.world();
        if(!world.running()) throw std::logic_error("World is not running");
        if(now<next_) return PrimaryNpcIdleResult::skipped;
        bool complete=true;
        const bool visited=root.visit_active_zones([&](RuntimeZone& zone){
            if(!dispatch_idle(world,zone,now)) complete=false;
        });
        if(!visited || !complete) return PrimaryNpcIdleResult::interrupted;
        next_=now+3000U;return PrimaryNpcIdleResult::completed;
    }
    // CZone::UpdateNpcAttack 00422390. Called within the paired 500 ms
    // mob/NPC attack pass; this method does not own a separate deadline.
    bool update_attack(VerticalWorld& world,RuntimeZone& zone,std::uint32_t now) {
        if(!world.running()) throw std::logic_error("World is not running");
        if(!zone.npcs) throw std::logic_error("Primary zone has no retained NPC owner");
        bool complete=true;
        world.serialized_update([&](VerticalWorld& current){
            auto bindings=services_->bindings(zone);
            if(!current.running()) {complete=false;return;}
            zone.npcs->visit_all(bindings.pool,[&](Npc& npc){
                if(!current.running()) {complete=false;return;}
                if(npc.disable_time<=now && npc.type==8 && npc.source)
                    services_->update_guard(zone,npc,now);
                if(!current.running()) complete=false;
            });
        });
        return complete;
    }
    std::uint32_t next_deadline() const noexcept {return next_;}
private:
    bool dispatch_idle(VerticalWorld& world,RuntimeZone& zone,std::uint32_t now) {
        if(!world.running()) return false;
        if(!zone.npcs) throw std::logic_error("Active zone has no retained NPC owner");
        auto bindings=services_->bindings(zone);
        if(!world.running()) return false;
        zone.npcs->visit_all(bindings.pool,[&](Npc& npc){
            if(!world.running()) return;
            if(npc.disable_time<=now && npc.source) {
                if(npc.type==8) services_->update_guard(zone,npc,now);
                else update_npc_patrol(npc,now,bindings.random,*zone.npcs);
            }
        });
        return world.running();
    }
    std::shared_ptr<PrimaryNpcIdleServices> services_;std::uint32_t next_=0;
};
}
