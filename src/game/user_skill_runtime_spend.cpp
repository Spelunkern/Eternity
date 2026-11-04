#include "eternity/game/user_skill_runtime_spend.hpp"
#include <algorithm>
#include <bit>
namespace eternity::game {
UserSkillSpendTransition user_skill_runtime_spend_transition(std::uint32_t sp,std::uint32_t mp,
    std::uint16_t sc,std::uint16_t mc,std::int32_t sr,std::int32_t mr){
    // Both x87 calculations execute even for the raw-cost-one sentinel.
    const auto sd=user_skill_runtime_resource_cost(sc,sr);
    const auto md=user_skill_runtime_resource_cost(mc,mr);
    const auto s=std::max<std::int32_t>(0,sc==1?std::bit_cast<std::int32_t>(sp):sd);
    const auto m=std::max<std::int32_t>(0,mc==1?std::bit_cast<std::int32_t>(mp):md);
    const auto ns=std::bit_cast<std::int32_t>(sp)>s?sp-static_cast<std::uint32_t>(s):0u;
    const auto nm=std::bit_cast<std::int32_t>(mp)>m?mp-static_cast<std::uint32_t>(m):0u;
    const auto ds=static_cast<std::uint16_t>(sp-ns),dm=static_cast<std::uint16_t>(mp-nm);
    return {ns,nm,s,m,{0x0c,0x05,static_cast<std::uint8_t>(ds),static_cast<std::uint8_t>(ds>>8),static_cast<std::uint8_t>(dm),static_cast<std::uint8_t>(dm>>8)}};
}
}
