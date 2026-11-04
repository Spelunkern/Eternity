#pragma once
#include "eternity/game/mob_combat_scheduler_actor.hpp"
#include "eternity/game/zone_users.hpp"
namespace eternity::game {
class MobCombatSchedulerZoneServices {
public:
    virtual ~MobCombatSchedulerZoneServices()=default;
    virtual std::shared_ptr<void> retain(Actor&)=0;
    // Pure per-actor context access under the acquired lease; no membership
    // mutation. Views, actual world and their owners survive the complete poll.
    virtual MobCombatSchedulerBindings& bindings(Actor&)=0;
    virtual MobCombatSchedulerActorWorld& world(Actor&)=0;
};
// CZone::UpdateUserAttack 00421930: one supplied tick, live global cursor, and
// ignored per-user bool. Zone/world serialization and actor leases are required.
void mob_combat_scheduler_zone(ZoneUsers&,std::uint32_t tick,MobCombatSchedulerZoneServices&);
}
