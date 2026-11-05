#pragma once
#include "eternity/game/vertical_combat_death.hpp"
#include "eternity/game/party_runtime_membership.hpp"
#include <stdexcept>
namespace eternity::game {
// CUser::AttackMob 0044cf04 / 0044ff80 reads the live party pointer and ID,
// then +1230 (recovery). Actor, its actual PartyMembershipUser, all captured
// party owners and death services must be retained and serialized across every
// synchronous callback. Root must pair the actual membership with this actor;
// matching IDs alone cannot prove ownership.
class VerticalCombatActorHitServices:public VerticalCombatDeathServices {
public:
 VerticalCombatActorHitServices(Actor& actor,PartyMembershipUser& membership,const MobDeathExperience& rules,MobDeathWorld& world)
  :VerticalCombatDeathServices(rules,world),actor_(actor),membership_(membership){require_actor(actor);}
 std::optional<std::uint32_t> party_id(const Actor& actor) final {
  require_actor(actor);if(!membership_.party)return {};return membership_.party->id;
 }
 std::int32_t aggro_bonus(const Actor& actor) final {
  require_actor(actor);return actor.status().skills.equipment.base.attributes.recovery;
 }
private:
 void require_actor(const Actor& actor)const {
  if(&actor!=&actor_ || membership_.id!=actor_.id())throw std::invalid_argument("Combat membership requires its retained actor");
 }
 Actor& actor_;PartyMembershipUser& membership_;
};
}
