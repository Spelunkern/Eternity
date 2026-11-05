#include "eternity/game/user_skill_runtime_announcement.hpp"
namespace eternity::game {
void user_skill_runtime_announcement(Actor& source,const UserSkillDefinition& info,
    std::uint8_t reason,std::uint32_t target,std::uint8_t mob,UserSkillAnnouncementWorld& world){
    net::Packet packet(20,0);
    const auto put=[&](unsigned at,std::uint32_t value,unsigned n){for(unsigned i=0;i<n;++i)packet[at+i]=static_cast<std::uint8_t>(value>>(8*i));};
    put(0,mob?0x517:0x511,2);packet[2]=reason;put(3,source.id(),4);
    put(7,target,4);put(11,info.effect.id,2);packet[13]=info.effect.level;
    if(source.status().in_zone)world.send(source,packet,source.id(),mob?0:target);
}
void UserSkillStrikeAnnouncementWorld::announce_multi(Actor& source,Actor& target,const UserSkillDefinition& info){
    user_skill_runtime_announcement(source,info,4,target.id(),0,world_);
}
}
