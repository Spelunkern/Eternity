#pragma once
#include "eternity/game/user_skill_runtime_strike.hpp"
namespace eternity::game {
class UserSkillAnnouncementWorld {
public:
    virtual ~UserSkillAnnouncementWorld()=default;
    // Actual captured-zone PSendView00421220: radius60/channel5, centered on
    // source's existing position. Excludes these two IDs; second is0 for mobs.
    // Same actual zone/Actor/scene must remain retained through synchronous send.
    virtual void send(Actor&,const net::Packet&,std::uint32_t exclude_source,
        std::uint32_t exclude_target)=0;
};
// SendUseSkill0044e900. Reads definition IDs (not runtime IDs). Flag is original
// low BYTE of its DWORD argument; no effects, state changes or self-send.
void user_skill_runtime_announcement(Actor&,const UserSkillDefinition&,
    std::uint8_t reason,std::uint32_t target_id,std::uint8_t mob_flag,
    UserSkillAnnouncementWorld&);
// Concrete announce port for selected multihit, leaving actual hit/damage/death
// and area providers required. No default view/target/damage implementations.
class UserSkillStrikeAnnouncementWorld:public UserSkillStrikeWorld {
public:
    explicit UserSkillStrikeAnnouncementWorld(UserSkillAnnouncementWorld& world):world_(world){}
    void announce_multi(Actor&,Actor&,const UserSkillDefinition&) final;
private:
    UserSkillAnnouncementWorld& world_;
};
}
