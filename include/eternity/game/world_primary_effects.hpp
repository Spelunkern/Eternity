#pragma once
#include "eternity/game/actor.hpp"
#include "eternity/game/vertical_world.hpp"
#include "eternity/game/zone_users.hpp"

namespace eternity::game {
struct PrimaryEffectAdminDeadlines {std::uint32_t& enable_chat;std::uint32_t& enable_move;};
class PrimaryEffectServices {
public:
    virtual ~PrimaryEffectServices()=default;
    // Pin the actual actor, session, admin state and transports through all
    // callbacks. Return references to the retained CUser admin timer owner.
    virtual std::shared_ptr<void> retain_effect_user(Actor&)=0;
    virtual void update_timeovers(RuntimeZone&,Actor&,std::uint32_t)=0;
    virtual PrimaryEffectAdminDeadlines effect_admin_deadlines(Actor&)=0;
    virtual void update_admin_notice(RuntimeZone&,Actor&,std::uint32_t)=0;
};
// ps_game.exe CZone::UpdateApplyEffect 004220a0. Caller serializes the
// retained world and zone, and the full timeout provider must perform all of
// CUser::UpdateTimeover, including exchange, social and vehicle operations.
inline bool update_primary_effects(VerticalWorld& world,RuntimeZone& zone,
    PrimaryEffectServices& services,std::uint32_t now) {
    if(!world.running()) return false;
    if(!zone.users) throw std::logic_error("Primary zone lacks effect user owner");
    zone.users->visit_all([&](Actor& actor){return services.retain_effect_user(actor);},[&](Actor& actor){
        if(!world.running() || actor.attack_state().mode==1) return;
        services.update_timeovers(zone,actor,now);
        if(!world.running()) return;
        auto deadlines=services.effect_admin_deadlines(actor);
        if(!world.running()) return;
        if(deadlines.enable_chat!=0 && deadlines.enable_chat<now) {
            deadlines.enable_chat=0;
            actor.send_self(net::Packet{0x0e,0xfa});
        }
        if(!world.running()) return;
        if(deadlines.enable_move!=0 && deadlines.enable_move<now) {
            deadlines.enable_move=0;
            actor.speed_changed(actor.status());
            if(!world.running()) return;
            actor.send_self(net::Packet{0x10,0xfa});
        }
        if(world.running()) services.update_admin_notice(zone,actor,now);
    });
    return world.running();
}
}
