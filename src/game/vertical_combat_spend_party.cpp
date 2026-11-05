#include "eternity/game/vertical_combat_spend_party.hpp"
#include <stdexcept>
namespace eternity::game {
namespace {
class PartyGroup final:public UserSkillSpendGroup {
public:
 PartyGroup(std::shared_ptr<PartyNormalMembership> p,PartyMembershipWorld& w):party(std::move(p)),world(w){}
 void lock() override{world.lock(*party);}
 void unlock() noexcept override{world.unlock(*party);}
 std::uint8_t kind() const override{return party->experience.kind;}
 std::shared_ptr<PartyNormalMembership> party;PartyMembershipWorld& world;
};
}
VerticalCombatSpendPartyWorld::VerticalCombatSpendPartyWorld(Actor& a,PartyMembershipUser& u,PartyMembershipWorld& w,VerticalCombatPartyLease& l):actor_(a),user_(u),world_(w),lease_(l){
 if(a.id()!=u.id)throw std::invalid_argument("Skill spend membership requires its actual actor");
}
UserSkillSpendGroupLease VerticalCombatSpendPartyWorld::membership(Actor& actor){
 if(&actor!=&actor_ || actor.id()!=user_.id)throw std::invalid_argument("Skill spend group bound to another actor");
 auto* party=user_.party;if(!party)return {};
 auto held=lease_.retain(*party);
 if(!held || held.get()!=party || user_.party!=party)throw std::logic_error("Skill spend requires the exact retained live party");
 return std::make_shared<PartyGroup>(std::move(held),world_);
}
void VerticalCombatSpendPartyWorld::send(UserSkillSpendGroup& group,Actor& actor,const net::Packet& packet){
 auto* retained=dynamic_cast<PartyGroup*>(&group);
 if(&actor!=&actor_ || !retained || &retained->world!=&world_ || user_.party!=retained->party.get())throw std::logic_error("Skill spend group dispatch identity mismatch");
 actor.send_party(packet);
}
}
