#pragma once
#include "eternity/game/mob_combat_scheduler_skills.hpp"
namespace eternity::game {
class MobCombatSchedulerUserWorld:public MobCombatSchedulerSkillWorld {
public:
    // FindUser in the requester's retained zone, not a global ID lookup.
    virtual Actor* find_attack_user(Actor&,std::uint32_t id)=0; // 00415f30
    // Pure live zone-definition query for existing same_team; no callbacks.
    virtual std::int32_t map_team_mode(const Actor&) const=0;
    virtual bool in_user_range(Actor&,Actor&)=0; // 0044bf90, argument zero
    virtual void attack_user(Actor&,Actor&)=0; // normal AttackUser 00456870
    bool normal_user_attack(Actor&,std::uint32_t now) final;
};
}
