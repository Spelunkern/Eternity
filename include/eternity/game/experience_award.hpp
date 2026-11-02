#pragma once
#include "eternity/game/stats.hpp"
namespace eternity::game {
struct ExperienceAwardOptions {
    bool skip_modifiers=false,double_bonus=false,half_bonus=false,normal_party_seven=false;
    std::int32_t bonus_mode=0;
};
class ExperienceAwardPorts {
public:
    virtual ~ExperienceAwardPorts()=default;
    virtual void send_database(const net::Packet&)=0;
    virtual void send_self(const net::Packet&)=0;
    // CUser::LevelUp: attack rebuild, DB/scene/party/guild notices and recovery.
    virtual void level_up(PlayerCore&,BaseStatus&,std::uint32_t amount,std::uint32_t source)=0;
};
// ps_game.exe CUser::AddExpFromUser 00458240; source is the first stack argument.
void award_experience(PlayerCore&,BaseStatus&,const StatusRules&,const Experience&,std::uint32_t source,std::int32_t amount,ExperienceAwardOptions,ExperienceAwardPorts&);
}
