#include "eternity/game/mob_combat_scheduler_zone.hpp"
namespace eternity::game {
void mob_combat_scheduler_zone(ZoneUsers& zone,std::uint32_t tick,MobCombatSchedulerZoneServices& services) {
    zone.visit_all([&](Actor& actor){return services.retain(actor);},[&](Actor& actor){
        auto& bindings=services.bindings(actor);auto& world=services.world(actor);
        static_cast<void>(mob_combat_scheduler(actor,bindings,tick,world));
    });
}
}
