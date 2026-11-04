#include "eternity/game/mob_ai_states_system.hpp"
#include <algorithm>
#include <stdexcept>
namespace eternity::game {
MobAiParameter MobAiMobQueries::invoke(MobAiConfiguredFsm& owner,std::uint32_t function){
    if(&owner!=&owner_)throw std::logic_error("Mob AI system dispatcher requires its actual bound instance");
    MobAiParameter result(1);
    switch(function){
    case 0x0ff3a2fdU:result.integer=subject_.in_battle;break;
    case 0x36824cb7U:result.type=3;result.dword=subject_.current_time;break;
    case 0x368302a7U:result.type=3;result.dword=subject_.target_kind==0?0:subject_.target_id;break;
    case 0xa45c668bU:result.integer=subject_.attacked;break;
    case 0xaec1b010U:result.integer=subject_.attack_at_moment;subject_.attack_at_moment=false;break;
    case 0xbbfc6d86U:subject_.attacked_count=std::max(subject_.attacked_count,0);result.integer=subject_.attacked_count;break;
    default:throw std::logic_error("Mob AI system function requires an unrecovered world provider");
    }
    return result;
}
}
