#include "eternity/game/mob_targeting_lifecycle_route.hpp"
#include "eternity/game/actor.hpp"
namespace eternity::game {
MobTargetingPlayerDeathRoute mob_targeting_player_death_route(
    const MobTargetingPvpRelation& victim,const MobTargetingPvpRelation& attacker,
    bool same_user,std::uint8_t forced_pvp,std::int32_t& penalty) noexcept {
    // Death 0045885c..004588bd, not PvPGetDeathType's symmetric guild rules.
    if (forced_pvp!=0) {
        penalty=-1; // Published even when no special defeat is selected.
        if (victim.duel_state==4 && attacker.duel_peer==victim.id &&
            victim.duel_peer==attacker.id) return MobTargetingPlayerDeathRoute::duel;
        if (victim.opposing_guild!=0) return MobTargetingPlayerDeathRoute::guild;
        return MobTargetingPlayerDeathRoute::skip_rewards;
    }
    const auto type=mob_targeting_pvp_death_type(victim,attacker,same_user);
    // Unforced special defeat also reaches Death 00458cac before the call.
    if (type!=MobTargetingPvpDeathType::ordinary) penalty=-1;
    return static_cast<MobTargetingPlayerDeathRoute>(type);
}
MobTargetingPvpRelation mob_targeting_pvp_relation(const Actor& actor) noexcept {
    const auto& r=actor.relations();
    return {actor.id(),static_cast<std::uint32_t>(r.duel_state),r.duel_opponent,
        r.guild_id,r.opposing_guild_id};
}
MobTargetingPvpDeathType mob_targeting_pvp_death_type(const Actor& victim,const Actor& attacker) noexcept {
    return mob_targeting_pvp_death_type(mob_targeting_pvp_relation(victim),
        mob_targeting_pvp_relation(attacker),&victim==&attacker);
}
MobTargetingPlayerDeathRoute mob_targeting_player_death_route(
    const Actor& victim,const Actor& attacker,std::uint8_t forced_pvp,
    std::int32_t& penalty) noexcept {
    return mob_targeting_player_death_route(mob_targeting_pvp_relation(victim),
        mob_targeting_pvp_relation(attacker),&victim==&attacker,forced_pvp,penalty);
}
}
