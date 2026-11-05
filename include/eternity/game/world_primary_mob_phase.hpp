#pragma once
#include "eternity/game/vertical_world.hpp"
#include "eternity/game/vertical_mobs.hpp"

namespace eternity::game {
struct PrimaryMobBindings {
    RandomSource& random;
    VerticalMobsWorld& idle;
    MobInitPorts& init;
    LinkedMobWorld& linked;
};
class PrimaryMobPhaseServices {
public:
    virtual ~PrimaryMobPhaseServices()=default;
    // These are the actual owners bound to this retained zone. In particular,
    // trap updates must not be replaced with an empty success provider.
    virtual PrimaryMobBindings bindings(RuntimeZone&)=0;
    // Pin this actual mob against pool release/reuse; acquisition must not
    // change membership. Required for the full active-list idle pass.
    virtual std::shared_ptr<void> retain_idle_mob(MobRuntime&)=0;
    virtual void update_traps(RuntimeZone&,std::uint32_t)=0;
};
enum class PrimaryMobPhaseResult {skipped,completed,interrupted};
// One phase of CWorldThread::Update 00407390, 00407623..00407678.
// The owner invokes this in original phase order on its world strand. The
// source visits the thread's full active-zone list; this helper visits only
// retained primary zones in their current map order and is not a full update.
class WorldPrimaryMobPhase final {
public:
    explicit WorldPrimaryMobPhase(std::shared_ptr<PrimaryMobPhaseServices> services):services_(std::move(services)) {
        if(!services_) throw std::invalid_argument("Primary mob phase requires real zone and trap services");
    }
    PrimaryMobPhaseResult update(VerticalWorld& world,std::uint32_t now) {
        if(!world.running()) throw std::logic_error("World is not running");
        if(now<next_) return PrimaryMobPhaseResult::skipped; // unsigned original comparison
        auto result=PrimaryMobPhaseResult::completed;
        world.serialized_update([&](VerticalWorld& current){
            current.visit_primary_zones_indexed([&](std::size_t id,RuntimeZone& zone){
                auto services=services_->bindings(zone);
                if(!current.running()) {result=PrimaryMobPhaseResult::interrupted;return;}
                if(!current.update_primary_mob_idle(id,now,services.random,services.idle,services.init,services.linked)) {
                    result=PrimaryMobPhaseResult::interrupted;return;
                }
                if(!current.running()) {result=PrimaryMobPhaseResult::interrupted;return;}
                services_->update_traps(zone,now);
                if(!current.running()) result=PrimaryMobPhaseResult::interrupted;
            });
        });
        if(result==PrimaryMobPhaseResult::completed) next_=now+1000U;
        return result;
    }
    // Full active-list alternative. Shares the primary-only deadline and
    // uses each retained instance's actual idle/death/respawn owners.
    template<class Root> PrimaryMobPhaseResult update_active(Root& root,std::uint32_t now) {
        auto& world=root.world();
        if(!world.running()) throw std::logic_error("World is not running");
        if(now<next_) return PrimaryMobPhaseResult::skipped;
        auto result=PrimaryMobPhaseResult::completed;
        const bool visited=root.visit_active_zones([&](RuntimeZone& zone){
            if(!world.running()){result=PrimaryMobPhaseResult::interrupted;return;}
            if(!zone.map || !zone.mobs || !zone.linked_mobs || !zone.mob_population)
                throw std::logic_error("Active zone lacks retained mob owners");
            auto bindings=services_->bindings(zone);
            if(!world.running()){result=PrimaryMobPhaseResult::interrupted;return;}
            if(&bindings.random!=&bindings.init.random) throw std::invalid_argument("Mob idle and initialization require the same random stream");
            auto authority=world.zones();
            zone.mobs->visit_global([&](MobRuntime& mob){
                if(!world.running()) return;
                auto pin=services_->retain_idle_mob(mob);
                if(!pin) throw std::logic_error("Unretained mob idle subject");
                if(world.running()) vertical_mobs_idle(mob,now,bindings.idle);
            });
            if(!world.running()){result=PrimaryMobPhaseResult::interrupted;return;}
            zone.mobs->update_deaths(authority->mob_pool(),bindings.random,now,*zone.linked_mobs);
            if(!world.running()){result=PrimaryMobPhaseResult::interrupted;return;}
            struct Interrupted {};
            try {
                zone.update_mob_population(authority->mob_pool(),bindings.init,now,
                    [&](std::uint32_t tick){
                        if(!world.running()) throw Interrupted{};
                        if(!authority->tick(authority->map_index(*zone.map),tick)) {
                            if(!world.running()) throw Interrupted{};
                            throw std::logic_error("Active primary obelisk update unavailable");
                        }
                    },
                    [&](std::uint32_t tick){
                        if(!world.running()) throw Interrupted{};
                        zone.linked_mobs->update(zone.map->definition,*zone.mobs,authority->mob_pool(),bindings.init,tick,bindings.linked);
                    });
            } catch(const Interrupted&) {result=PrimaryMobPhaseResult::interrupted;return;}
            if(!world.running()){result=PrimaryMobPhaseResult::interrupted;return;}
            services_->update_traps(zone,now);
            if(!world.running())result=PrimaryMobPhaseResult::interrupted;
        });
        if(!visited || result==PrimaryMobPhaseResult::interrupted) return PrimaryMobPhaseResult::interrupted;
        next_=now+1000U;return PrimaryMobPhaseResult::completed;
    }
    std::uint32_t next_deadline() const noexcept {return next_;}
private:
    std::shared_ptr<PrimaryMobPhaseServices> services_;
    std::uint32_t next_=0; // CWorldThread constructor 00407100.
};
}
