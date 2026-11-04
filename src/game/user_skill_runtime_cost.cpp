#include "eternity/game/user_skill_runtime_cost.hpp"
#include <bit>
#include <cfenv>
#include <limits>
#include <stdexcept>
namespace eternity::game {
std::int32_t user_skill_runtime_resource_cost(std::uint16_t cost,std::int32_t reduction){
    if(std::fegetround()!=FE_TONEAREST)throw std::logic_error("Skill cost requires recovered CRT nearest rounding");
    // Original FILD/FSTP32 operands, then x87 precision53 FDIV and FMUL.
    // Volatile stages prevent contraction/extended intermediate retention.
    const float amount=static_cast<float>(cost),modifier=static_cast<float>(reduction);
    volatile double fraction=static_cast<double>(amount)/100.0;
    volatile double product=fraction*static_cast<double>(modifier);
    const auto discount=product>=2147483648.0||product< -2147483648.0?std::numeric_limits<std::int32_t>::min():static_cast<std::int32_t>(product);
    return std::bit_cast<std::int32_t>(std::uint32_t(cost)-std::bit_cast<std::uint32_t>(discount));
}
bool user_skill_runtime_check_resources(UserSkillRuntime& owner,UserSkillHandle skill,const UserSkillDefinition& info,bool mob){
    if(!skill)throw std::invalid_argument("Resource validation requires retained actual runtime");
    auto& actor=owner.actor();const auto& benefits=actor.status().skills;
    if(std::bit_cast<std::int32_t>(actor.resources().sp)>=user_skill_runtime_resource_cost(info.sp_cost,benefits.sp_cost_reduction)&&
       std::bit_cast<std::int32_t>(actor.resources().mp)>=user_skill_runtime_resource_cost(info.mp_cost,benefits.mp_cost_reduction))return true;
    net::Packet packet(20,0);const auto put=[&](unsigned at,std::uint32_t v,unsigned n){for(unsigned i=0;i<n;++i)packet[at+i]=static_cast<std::uint8_t>(v>>(8*i));};
    put(0,mob?0x517:0x511,2);packet[2]=6;put(3,actor.id(),4);put(11,skill->skill_id,2);packet[13]=static_cast<std::uint8_t>(skill->level);actor.send_self(packet);return false;
}
}
