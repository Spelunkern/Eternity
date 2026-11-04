#pragma once
#include "eternity/game/mob_targeting_zone.hpp"
#include "eternity/game/random.hpp"
namespace eternity::game {
class Actor;
class ZoneUsers;
class MobAiStateServices;
// CUser+142c is the shared life/action state, represented by the existing
// ActorAttackState::mode. Preserve this interface for explicit provider users;
// normal production assembly uses the concrete mode delegation below.
class MobTargetingActorLife {
public:
    virtual ~MobTargetingActorLife()=default;
    virtual std::int32_t life_status(const Actor&) const=0;
};
class MobTargetingActorModeLife final:public MobTargetingActorLife {
public:
    std::int32_t life_status(const Actor&) const override;
};
// Borrows a retained canonical Actor and its life owner; reprojects every read.
// Both must outlive all synchronous help/target callbacks on the zone executor.
class MobTargetingActorUser final:public MobTargetingZoneUser {
public:
    explicit MobTargetingActorUser(const Actor&); // Existing shared +142c owner.
    MobTargetingActorUser(const Actor&,const MobTargetingActorLife&);
    MobTargetingUser target() const override;
    MobTargetingHelpUser help() const override;
private:
    const Actor& actor_;const MobTargetingActorLife& life_;
};
// Bind the EXISTING paired mob/user grids, shared per-executor MSVCRT stream,
// and actual fixed stack FSM services (clock/Lua/errors/update). No new owners
// or locks. Call on their common serialized execution context and retain every
// dependency/subject through callbacks. Configured AI remains the existing
// configured owner behind its registered handlers/Lua services, not a second
// event-to-state table fabricated here.
class MobTargetingActorWorld final:public MobTargetingZoneWorld {
public:
    MobTargetingActorWorld(ZoneMobs&,const ZoneUsers&,MicrosoftRandom&,MobAiStateServices&);
    std::int32_t user_count(const ZoneMobs&) const override;
    std::int32_t random(MobRuntime&) override;
    std::uint32_t tick_ms(MobRuntime&) override;
    void dispatch(MobRuntime&,std::uint32_t event,std::uint32_t argument) override;
private:
    ZoneMobs& mobs_;const ZoneUsers& users_;MicrosoftRandom& random_;MobAiStateServices& fsm_;
};
}
