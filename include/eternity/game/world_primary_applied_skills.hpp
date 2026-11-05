#pragma once
#include "eternity/game/actor.hpp"
#include "eternity/game/vertical_world.hpp"
#include "eternity/game/mob_applied_skills_catalog.hpp"

namespace eternity::game {
class PrimaryAppliedSkillServices {
public:
    virtual ~PrimaryAppliedSkillServices()=default;
    // Tokens pin the actual actor/session or mob through synchronous callbacks,
    // including pool release/reuse. The item definitions are the actor's bound
    // definitions and remain valid through the complete expiration operation.
    virtual std::shared_ptr<void> retain_skill_actor(Actor&)=0;
    virtual const ItemDefinitions& actor_items(Actor&)=0;
    virtual void update_actor_recover(RuntimeZone&,Actor&,std::uint32_t)=0;
    virtual void update_actor_pvp_timeout(RuntimeZone&,Actor&,std::uint32_t)=0;
    virtual void update_actor_rebirth(RuntimeZone&,Actor&,std::uint32_t)=0;
    virtual std::shared_ptr<void> retain_skill_mob(MobRuntime&)=0;
    virtual void update_mob_recover(RuntimeZone&,MobRuntime&,std::uint32_t)=0;
};
// CZone::UpdateApplySkill 00421f80: users first, then mobs; callers serialize
// the zone and must retain real definitions, transport and actor owners.
inline bool update_primary_applied_skills(VerticalWorld& world,RuntimeZone& zone,
    PrimaryAppliedSkillServices& services,std::uint32_t now) {
    if(!world.running()) return false;
    if(!zone.users || !zone.mobs) throw std::logic_error("Primary zone lacks skill pass owners");
    zone.users->visit_all([&](Actor& actor){return services.retain_skill_actor(actor);},[&](Actor& actor){
        if(!world.running()) return;
        services.update_actor_recover(zone,actor,now);
        if(!world.running()) return;
        if(!actor.effect_database_bound()) throw std::logic_error("Actor has no effect database owner");
        const auto& items=services.actor_items(actor);
        if(!world.running()) return;
        EffectEquipment equipment;
        for(const auto& item:actor.inventory()) if(item.bag==0) {
            if(item.slot==5) equipment.weapon=&item;else if(item.slot==1) equipment.armor=&item;
        }
        actor.effects().expire(now,actor.status(),equipment,items,actor,actor);
        if(!world.running()) return;
        services.update_actor_pvp_timeout(zone,actor,now);
        if(world.running()) services.update_actor_rebirth(zone,actor,now);
    });
    if(!world.running()) return false;
    zone.mobs->visit_global([&](MobRuntime& mob){
        if(!world.running()) return;
        auto pin=services.retain_skill_mob(mob);
        if(!pin) throw std::logic_error("Unretained skill-pass mob");
        if(!world.running()) return;
        services.update_mob_recover(zone,mob,now);
        if(world.running()) mob.applied_skills().expire(now);
    });
    return world.running();
}
}
