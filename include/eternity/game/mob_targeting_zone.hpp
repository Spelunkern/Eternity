#pragma once
#include "eternity/game/mob_targeting_bindings.hpp"
#include "eternity/game/mob_targeting_help.hpp"
#include "eternity/game/zone_mobs.hpp"

namespace eternity::game {
// Live non-owning actor boundary. The real user owner supplies these projections
// and retains that user for the complete synchronous call, including reentry.
class MobTargetingZoneUser {
public:
    virtual ~MobTargetingZoneUser()=default;
    virtual MobTargetingUser target() const=0;
    virtual MobTargetingHelpUser help() const=0;
};
class MobTargetingZoneWorld {
public:
    virtual ~MobTargetingZoneWorld()=default;
    virtual std::int32_t user_count(const ZoneMobs&) const=0;
    virtual std::int32_t random(MobRuntime&)=0;
    virtual std::uint32_t tick_ms(MobRuntime&)=0;
    virtual void dispatch(MobRuntime&,std::uint32_t event,std::uint32_t argument)=0;
};
// Borrows the EXISTING zone and full catalog. The zone owner serializes the
// complete call and reentry on its existing execution context. If that owner
// uses locks, exclusion must permit synchronous reentry; this adds no mutex.
// Retain source and every SetTarget subject through its call, even if detached.
// Do not recycle/release the zone or replace/reload the catalog concurrently.
class MobTargetingZone {
public:
    MobTargetingZone(ZoneMobs&,const MobTargetingBindingCatalog&,MobTargetingZoneWorld&);
    void set_target(MobRuntime&,const MobTargetingZoneUser&,bool initialize);
    void find_help(MobRuntime&,const MobTargetingZoneUser&,MobTargetingHelpKind);
private:
    class Runtime;
    class Help;
    const MobTargetingBindingDefinition& definition(const MobRuntime&) const;
    ZoneMobs& zone_;
    const MobTargetingBindingCatalog& catalog_;
    MobTargetingZoneWorld& world_;
};
}
