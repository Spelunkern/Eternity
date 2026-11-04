#pragma once
#include "eternity/game/experience_award.hpp"
namespace eternity::game {
class Actor;
// CUser::GetExp 004581b0. Original masked nearest/53-bit x87 environment.
// Raw subtraction wraps at 32 bits; FISTP retains its low DWORD before max(1).
std::int32_t mob_targeting_user_death_experience(std::uint16_t victim_level,std::int32_t recipient_level);
class MobTargetingSoloDeathServices {
public:
    virtual ~MobTargetingSoloDeathServices()=default;
    virtual std::uint16_t victim_level() const noexcept=0;
    virtual std::uint16_t recipient_level() const noexcept=0;
    virtual void award(std::uint32_t source,std::int32_t amount)=0;
    virtual void kill_from()=0; // KillPCFrom 0045b050, after award returns.
    virtual void drop_by_user()=0; // ItemDropByUser 0045a180, even for zero award.
};
// Selected no-party reward branch of Death 004588cf..00458934, not full Death.
void mob_targeting_solo_death_reward(std::uint32_t source,MobTargetingSoloDeathServices&);
class MobTargetingSoloDeathWorld {
public:
    virtual ~MobTargetingSoloDeathWorld()=default;
    // Pure live queries over retained existing owners, no copied player/catalog.
    virtual bool has_party(const Actor&) const noexcept=0;
    virtual PlayerCore& player(Actor&) const noexcept=0;
    virtual const StatusRules& status_rules() const noexcept=0;
    virtual const Experience& experience_curve() const noexcept=0;
    virtual ExperienceAwardPorts& award_ports(Actor&) const noexcept=0;
    virtual void kill_from(Actor& recipient,Actor& victim)=0;
    virtual void drop_by_user(Actor& victim,Actor& recipient)=0;
};
// Delegates actual Actor::gain_experience with live skill benefit inputs.
// A party at entry is an explicit error before reward publication; caller must
// continue the separate party branch, never repeat Death's published prefix.
void mob_targeting_solo_death_reward(Actor& victim,Actor& recipient,std::uint32_t source,
    MobTargetingSoloDeathWorld&);
}
