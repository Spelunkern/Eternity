#pragma once
#include "eternity/game/vertical_world.hpp"
#include "eternity/game/mob_combat_scheduler_zone.hpp"

namespace eternity::game {
class PrimaryUserAttackServices {
public:
    virtual ~PrimaryUserAttackServices()=default;
    // Bind the actual zone's actor-retention, attack and world providers.
    virtual MobCombatSchedulerZoneServices& bindings(RuntimeZone&)=0;
};
enum class PrimaryUserAttackResult {skipped,completed,interrupted};
// CWorldThread::Update 00407390, user attack pass 004074aa..004074e9.
// Only primary zones are visited; the original also visits active instances.
// Caller invokes this on the world strand at its original phase position.
class WorldPrimaryUserAttackPhase final {
public:
    explicit WorldPrimaryUserAttackPhase(std::shared_ptr<PrimaryUserAttackServices> services):services_(std::move(services)) {
        if(!services_) throw std::invalid_argument("Primary user attack phase requires real combat services");
    }
    PrimaryUserAttackResult update(VerticalWorld& world,std::uint32_t now) {
        if(!world.running()) throw std::logic_error("World is not running");
        if(now<next_) return PrimaryUserAttackResult::skipped;
        auto result=PrimaryUserAttackResult::completed;
        world.serialized_update([&](VerticalWorld& current){
            current.visit_primary_zones([&](RuntimeZone& zone){
                if(!dispatch(current,zone,now)) result=PrimaryUserAttackResult::interrupted;
            });
        });
        if(result==PrimaryUserAttackResult::completed) next_=now+100U;
        return result;
    }
    // Active-list version of the same original phase. Both entry points share
    // one deadline; callers choose exactly one for each frame.
    template<class Root> PrimaryUserAttackResult update_active(Root& root,std::uint32_t now) {
        auto& world=root.world();
        if(!world.running()) throw std::logic_error("World is not running");
        if(now<next_) return PrimaryUserAttackResult::skipped;
        bool completed=true;
        const bool visited=root.visit_active_zones([&](RuntimeZone& zone){
            if(!dispatch(world,zone,now)) completed=false;
        });
        if(!visited || !completed) return PrimaryUserAttackResult::interrupted;
        next_=now+100U;return PrimaryUserAttackResult::completed;
    }
    std::uint32_t next_deadline() const noexcept {return next_;}
private:
    bool dispatch(VerticalWorld& world,RuntimeZone& zone,std::uint32_t now) {
        if(!world.running()) return false;
        if(!zone.users) throw std::logic_error("Active zone has no retained user owner");
        auto& bindings=services_->bindings(zone);
        if(!world.running()) return false;
        mob_combat_scheduler_zone(*zone.users,now,bindings);
        return world.running();
    }
    std::shared_ptr<PrimaryUserAttackServices> services_;std::uint32_t next_=0;
};
}
