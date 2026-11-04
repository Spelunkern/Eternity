#pragma once
#include "eternity/game/actor.hpp"
#include "eternity/game/mobs.hpp"
namespace eternity::game {
// Required real owners for CUser::AttackMob 0044ce10. All references, the
// equipped item, and mob identity must survive synchronous callbacks.
class VerticalCombatWorld {
public:
    virtual ~VerticalCombatWorld()=default;
    virtual void remove_sleep(MobRuntime&)=0; // CMob::RemApplySkillSleep 004a16c0
    // Borrow actual equipped weapon after sleep removal; absence/unbound is an error.
    virtual const ItemDefinition& weapon(Actor&)=0;
    virtual bool attack_success(Actor&,MobRuntime&,unsigned kind)=0; // 0044d010, null skill
    virtual bool attack_damage(Actor&,MobRuntime&,unsigned kind,std::array<std::int32_t,3>&)=0; // 0044d540, null skill; returns critical
    virtual void wear_weapon(Actor&)=0; // QualityByAttack 0045b260
    // Complete party-credit/Damage/stop/Death segment 0044cf04..0044cf61,
    // with the live mob-owned ledger and actual actor/party services.
    virtual bool apply_hit(Actor&,MobRuntime&,std::array<std::int32_t,3>&)=0;
    // Actual CZone::PSendView; implementation checks live zone and sends only if present.
    virtual void send_attack_view(Actor&,const net::Packet&)=0;
    virtual void set_target(MobRuntime&,Actor&)=0; // SetTarget 0049fad0, help=true
    virtual void attack_item_option(Actor&,MobRuntime&)=0; // 0044c720
};
// Recovered caller only; attack_success, attack_damage and apply_hit must be real
// implementations. This is not a complete playable attack path on its own.
void vertical_combat_attack_mob(Actor&,MobRuntime&,VerticalCombatWorld&);
}
