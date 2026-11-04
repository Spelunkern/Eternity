#include "eternity/game/mob_combat_attack.hpp"
namespace eternity::game {
bool mob_combat_user_hit(MobCombatState& mob,MobDamageLedger& ledger,MobCombatAttackServices& services,
                         MobCombatAttacker& user,const std::array<std::int32_t,3>& damage,
                         std::int32_t party_damage,std::int32_t base_aggro) {
    std::uint32_t party=0;
    if(user.party) {
        mob_combat_party(mob,ledger,services,*user.party,party_damage);
        // The caller reloads membership after DamageParty, irrespective of its
        // boolean result. Owners must retain a present party across this call.
        party=user.party.value();
    }
    const MobCombatCredit credit{1,user.id,party,user.aggro_bonus,base_aggro};
    const bool killed=mob_combat_damage(mob,ledger,services,damage,credit);
    if(killed) {
        services.stop_attack(user);
        services.death(mob,1,user.id); // Original reload after StatusAttackStop.
    }
    return killed;
}
}
