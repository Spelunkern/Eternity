#pragma once
#include "eternity/game/mob_targeting_lifecycle_pvp.hpp"
namespace eternity::game {
class Actor;
// Branch selected AFTER a nonnull attacker lookup/name copy inside Death.
// Includes special-defeat penalty publication at 00458cac.
// skip_rewards continues at 00458cbe; it is not a successful PvP defeat.
enum class MobTargetingPlayerDeathRoute { rewards=3,duel=4,guild=5,skip_rewards=0 };
MobTargetingPlayerDeathRoute mob_targeting_player_death_route(
    const MobTargetingPvpRelation& victim,const MobTargetingPvpRelation& attacker,
    bool same_user,std::uint8_t forced_pvp,std::int32_t& penalty) noexcept;
// Existing Actor/UserRelations are authoritative, including live guild IDs.
MobTargetingPvpRelation mob_targeting_pvp_relation(const Actor&) noexcept;
MobTargetingPvpDeathType mob_targeting_pvp_death_type(const Actor&,const Actor&) noexcept;
MobTargetingPlayerDeathRoute mob_targeting_player_death_route(
    const Actor& victim,const Actor& attacker,std::uint8_t forced_pvp,
    std::int32_t& authoritative_penalty) noexcept;
}
