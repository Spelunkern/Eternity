#pragma once
#include "eternity/game/user_skill_runtime_spend_group.hpp"
#include "eternity/game/party_runtime_membership.hpp"
namespace eternity::game {
// Must retain the exact live party and its lock; no membership mutations.
class VerticalCombatPartyLease {
public:
 virtual ~VerticalCombatPartyLease()=default;
 virtual std::shared_ptr<PartyNormalMembership> retain(PartyNormalMembership&)=0;
};
// Actor's scene must use the same PartyMembershipUser and actual party send
// owner. Actor, membership, scene, locks and leases survive every callback.
class VerticalCombatSpendPartyWorld final:public UserSkillSpendGroupWorld {
public:
 VerticalCombatSpendPartyWorld(Actor&,PartyMembershipUser&,PartyMembershipWorld&,VerticalCombatPartyLease&);
 UserSkillSpendGroupLease membership(Actor&) override;
 void send(UserSkillSpendGroup&,Actor&,const net::Packet&) override;
private:
 Actor& actor_;PartyMembershipUser& user_;PartyMembershipWorld& world_;VerticalCombatPartyLease& lease_;
};
}
