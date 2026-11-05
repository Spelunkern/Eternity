#pragma once
#include "eternity/game/vertical_world.hpp"
#include <memory>
#include <stdexcept>

namespace eternity::game {
class ZoneMobAttackServices {
public:
    virtual ~ZoneMobAttackServices()=default;
    // Pin the actual mob against pool release/reuse through its update;
    // acquiring the token must not change zone membership.
    virtual std::shared_ptr<void> retain_attack_mob(MobRuntime&)=0;
    // Must execute CMob::UpdateAttack on this subject and its actual owners.
    virtual void update_attack_mob(RuntimeZone&,MobRuntime&,std::uint32_t)=0;
};
// CZone::UpdateMobAttack 00421990 traverses the actual live mob list.
// The world/zone authority remains serialized through every callback.
inline bool update_zone_mob_attacks(VerticalWorld& world,RuntimeZone& zone,
    ZoneMobAttackServices& services,std::uint32_t now) {
    if(!world.running()) return false;
    if(!zone.mobs) throw std::logic_error("Active zone has no retained mob owner");
    zone.mobs->visit_global([&](MobRuntime& mob){
        if(!world.running()) return;
        auto pin=services.retain_attack_mob(mob);
        if(!pin) throw std::logic_error("Unretained mob attack subject");
        if(world.running()) services.update_attack_mob(zone,mob,now);
    });
    return world.running();
}
}
