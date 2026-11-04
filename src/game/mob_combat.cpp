#include "eternity/game/mob_combat.hpp"
#include <bit>
namespace eternity::game {
namespace {
std::int32_t signed_bits(std::uint32_t n) { return std::bit_cast<std::int32_t>(n); }
bool damage(MobCombatState& m,MobDamageLedger& ledger,MobCombatServices& svc,std::uint32_t hp,const std::array<std::int32_t,3>* triple,const MobCombatCredit& c) {
    if(svc.status(m)==2 || m.hp==0) return false;
    if(m.count_hits) ++m.hit_count;
    if(m.hp>hp) m.hp-=hp;
    else {
        m.hp=0;
        if(svc.status(m)!=2) {
            m.state=2;
            if(m.stacked_fsm) svc.dispatch_death_event(m);
        }
        m.secondary_state=0;
        const auto delay=m.long_corpse_delay?10000u:5000u;
        m.death_tick=svc.tick()+delay;
    }
    const auto current_damage=[&] {return triple?static_cast<std::uint32_t>((*triple)[0]):hp;};
    if(m.log_enabled || m.linked_log_enabled) svc.log_damage(m,current_damage());
    if(triple) {
        m.mp=static_cast<std::int32_t>(m.mp)<(*triple)[1]?0:static_cast<std::uint16_t>(m.mp-current_damage());
        m.sp=static_cast<std::int32_t>(m.sp)<(*triple)[2]?0:static_cast<std::uint16_t>(m.sp-static_cast<std::uint32_t>((*triple)[2]));
    }
    ledger.add(c.type,c.id,signed_bits(current_damage()),signed_bits(static_cast<std::uint32_t>(c.base_aggro)+static_cast<std::uint32_t>(c.bonus)*10u),c.party);
    if(m.hp==0) return true;
    if(svc.status(m)==1) svc.retarget(m,m.stacked_fsm);
    return false;
}
}
bool mob_combat_damage(MobCombatState& m,MobDamageLedger& l,MobCombatServices& s,std::uint32_t hp,const MobCombatCredit& c) {return damage(m,l,s,hp,nullptr,c);}
bool mob_combat_damage(MobCombatState& m,MobDamageLedger& l,MobCombatServices& s,const std::array<std::int32_t,3>& v,const MobCombatCredit& c) {return damage(m,l,s,static_cast<std::uint32_t>(v[0]),&v,c);}
bool mob_combat_party(MobCombatState& m,MobDamageLedger& l,MobCombatServices& s,std::uint32_t party,std::int32_t value) {
    if(s.status(m)==2 || m.hp==0) return false;
    l.add(5,party,value,0,0);return true;
}
}
