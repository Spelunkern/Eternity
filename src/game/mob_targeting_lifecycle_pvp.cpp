#include "eternity/game/mob_targeting_lifecycle_pvp.hpp"
namespace eternity::game {
MobTargetingPvpDeathType mob_targeting_pvp_death_type(
    const MobTargetingPvpRelation& subject,const MobTargetingPvpRelation& peer,
    bool same_user) noexcept {
    if (!same_user) {
        if (subject.duel_state==4 && subject.duel_peer==peer.id)
            return MobTargetingPvpDeathType::duel;
        if (subject.guild!=0 && peer.guild!=0 &&
            peer.guild==subject.opposing_guild && peer.opposing_guild==subject.guild)
            return MobTargetingPvpDeathType::guild;
    }
    return MobTargetingPvpDeathType::ordinary;
}
}
