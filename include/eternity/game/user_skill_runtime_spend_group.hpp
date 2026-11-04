#pragma once
#include "eternity/game/user_skill_runtime_spend.hpp"
namespace eternity::game {
// Actual retained party/raid owner adapter. No private membership or member list.
class UserSkillSpendGroup {
public:
    virtual ~UserSkillSpendGroup()=default;
    virtual void lock()=0;
    virtual void unlock() noexcept=0;
    virtual std::uint8_t kind() const=0;
};
using UserSkillSpendGroupLease=std::shared_ptr<UserSkillSpendGroup>;
class UserSkillSpendGroupWorld {
public:
    virtual ~UserSkillSpendGroupWorld()=default;
    // Side-effect-free borrow of actual membership; never repairs/remaps it.
    virtual UserSkillSpendGroupLease membership(Actor&)=0;
    // Must invoke actual CParty::Send semantics, excluding the source actor.
    virtual void send(UserSkillSpendGroup&,Actor&,const net::Packet&)=0;
};
// AFTER actual resource commit and self050c send. Reuses live Actor resources.
// Captured group lock spans notifications; live membership may differ from it.
// Invalid/null dispatch after callback is an explicit integration error.
void user_skill_runtime_spend_group(Actor&,const UserSkillSpendTransition&,
    UserSkillSpendGroupWorld&);
}
