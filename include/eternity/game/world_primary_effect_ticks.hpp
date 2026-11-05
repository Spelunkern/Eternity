#pragma once
#include "eternity/game/actor.hpp"
#include "eternity/game/vertical_world.hpp"
#include "eternity/game/mob_applied_skills_catalog.hpp"

namespace eternity::game {
class PrimaryEffectTickServices {
public:
    virtual ~PrimaryEffectTickServices()=default;
    // Tokens pin actor/session or mob through the synchronous callback,
    // including against release and pool reuse.
    virtual std::shared_ptr<void> retain_effect_actor(Actor&)=0;
    virtual void keep_actor_skills(RuntimeZone&,Actor&,std::uint32_t)=0;
    virtual std::shared_ptr<void> retain_effect_mob(MobRuntime&)=0;
};
// CZone::UpdateApplyEffectTick 004221d0: living users then living mobs.
// Caller holds the serialized world/zone authority and actual owner lifetimes.
inline bool update_primary_effect_ticks(VerticalWorld& world,RuntimeZone& zone,
    PrimaryEffectTickServices& services,std::uint32_t now) {
    if(!world.running()) return false;
    if(!zone.users || !zone.mobs) throw std::logic_error("Primary zone lacks effect tick owners");
    zone.users->visit_all([&](Actor& actor){return services.retain_effect_actor(actor);},[&](Actor& actor){
        if(world.running() && actor.attack_state().mode!=1) services.keep_actor_skills(zone,actor,now);
    });
    if(!world.running()) return false;
    zone.mobs->visit_global([&](MobRuntime& mob){
        if(!world.running() || mob_status(mob)==2) return;
        auto pin=services.retain_effect_mob(mob);
        if(!pin) throw std::logic_error("Unretained effect-tick mob");
        if(world.running() && mob_status(mob)!=2) mob.applied_skills().keep(now);
    });
    return world.running();
}
}
