#include "eternity/game/vertical_mobs.hpp"
#include <stdexcept>

namespace eternity::game {
void vertical_mobs_idle(MobRuntime& mob,std::uint32_t now,VerticalMobsWorld& world) {
    mob.current_time=now;
    if(mob.use_pais) {
        const auto status=mob_status(mob);
        if(status==100) {
            auto& fsm=world.configured_fsm(mob);
            auto& services=world.configured_services(mob);
            evaluate_mob_ai_configured(mob,fsm,services);
            execute_mob_ai_configured(mob,fsm,services);
            return;
        }
        if(status==3) { world.return_home(mob,now); return; }
    }
    if(mob_status(mob)==2) return;
    if(mob.state_stack && !poll_mob_ai_stack(mob,world.stack_services(mob))) {
        world.release_failed_stack(mob);
        return;
    }
    if(mob_status(mob)==0) world.ordinary_idle(mob,now);
}
bool vertical_mobs_idle_frame(MobPrimaryZones& root,std::size_t map,std::uint32_t now,
    RandomSource& random,VerticalMobsWorld& world) {
    auto lease=root.lease(map);
    if(!lease) return false;
    lease.visit([&](RuntimeZone& zone) {
        if(!zone.mobs || !zone.linked_mobs || !zone.mob_population || !zone.map)
            throw std::logic_error("Mob frame requires the actual initialized zone owners");
        zone.mobs->visit_global([&](MobRuntime& mob){ vertical_mobs_idle(mob,now,world); });
        zone.mobs->update_deaths(root.mob_pool(),random,now,*zone.linked_mobs);
    });
    return true;
}
}
