#include "eternity/game/mob_applied_skills_time_damage.hpp"
#include <algorithm>
#include <bit>

namespace eternity::game {
namespace {
std::int32_t converted(double value) noexcept {
    // All products in this routine fit int64. FISTP/_ftol2 truncate to int64,
    // and the original caller retains only its low DWORD.
    return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(static_cast<std::int64_t>(value)));
}
}
MobAppliedSkillTimeDamageResources project_mob_applied_skill_time_damage(const MobRuntime& mob,
    const MobRuntimeTemplate& definition) noexcept {
    return {mob.hp,definition.hp,mob.sp,mob.mp,definition.sp,definition.mp};
}
MobAppliedSkillDamage calculate_mob_applied_skill_time_damage(const MobAppliedSkillTimeDamageDefinition& definition,
    const MobAppliedSkillTimeDamageResources& resources,std::uint32_t elapsed_ms) noexcept {
    MobAppliedSkillDamage result{};
    const std::array<std::uint32_t,3> current{resources.hp,resources.sp,resources.mp};
    const std::array<std::uint32_t,3> maximum{resources.maximum_hp,resources.maximum_sp,resources.maximum_mp};
    for(std::size_t i=0;i<result.size();++i) {
        const auto value=definition.damage[i];
        switch(definition.type) {
        case 0:result[i]=value;break;
        case 3:
            if(value)result[i]=std::max(1,std::bit_cast<std::int32_t>(current[i]*value));
            break;
        case 4:case 5:
            if(value) {
                // Original FSTP float on the resource occurs BEFORE multiplying
                // by the extended-register percentage; no integer percentage rule.
                volatile float resource=static_cast<float>((definition.type==4?maximum:current)[i]);
                volatile double ratio=static_cast<double>(value)/100.0;
                volatile double product=ratio*static_cast<double>(resource);
                result[i]=std::max(1,converted(product));
            }
            break;
        default:break;
        }
    }
    volatile double factor=static_cast<double>(elapsed_ms)/3000.0;
    for(auto& value:result) {
        volatile double product=static_cast<double>(value)*factor;
        value=converted(product);
        if(value>30000)value=30000;
    }
    return result;
}
}
