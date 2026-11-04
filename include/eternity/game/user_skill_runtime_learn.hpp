#pragma once
#include "eternity/game/user_skill_runtime.hpp"
namespace eternity::game {
class UserSkillLearnWorld {
public:
    virtual ~UserSkillLearnWorld()=default;
    virtual std::uint32_t tick_ms()=0;
    // ChkGetSkillPassive 00489a40: real passive/stat/network owner, with the
    // requested definition still captured even if callbacks change a slot.
    virtual void apply_passive(UserSkillRuntime&,const UserSkillDefinition&)=0;
    virtual void send_database(const net::Packet&)=0;
    // SetGameLogMain 0045b0d0, shared log header bytes [2,74). Called AFTER
    // DB/client delivery; must populate actual current identity/time/position.
    virtual void populate_log(Actor&,std::span<std::uint8_t>)=0;
    virtual void send_log(const net::Packet&)=0;
};
}
