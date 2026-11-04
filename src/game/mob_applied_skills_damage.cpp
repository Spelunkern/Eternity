#include "eternity/game/mob_applied_skills_damage.hpp"
#include <bit>
#include <exception>

namespace eternity::game {
namespace {
void finish_hit(MobRuntime& mob,MobAppliedSkill& skill,bool periodic,std::uint32_t captured_hp,
                MobAppliedSkillDamage& damage,MobAppliedSkillDamageServices& services) noexcept {
    // IsBothmob 0044c610 runs after TimeDamage, once; preserve this branch even
    // if damage/death callbacks subsequently change the live flags.
    const bool named=mob.lua_ai!=0 || (mob.respawn && mob.respawn->both);
    if(named) {
        for(std::size_t i=0;i<mob.previous_attack_skill.size();++i) {
            const auto value=i<skill.name.size()?static_cast<std::uint8_t>(skill.name[i]):std::uint8_t{0};
            mob.previous_attack_skill[i]=value;
            if(value==0)break;
        }
    }
    const bool killed=periodic?
        services.damage_periodic(mob,skill.target_type,skill.target_id,damage):
        services.damage_hp(mob,skill.target_type,skill.target_id,captured_hp);
    if(killed)services.death(mob,skill.target_type,skill.target_id);
    if(named) {
        mob.previous_attack_user[0]=0;
        mob.previous_attack_skill[0]=0;
    }
    MobAppliedSkillDamagePacket packet{};
    std::size_t at=0;
    auto put=[&](std::uint32_t value,unsigned bytes) {
        for(unsigned i=0;i<bytes;++i)packet[at++]=static_cast<std::uint8_t>(value>>(i*8));
    };
    put(0x607,2);put(mob.id,4);put(skill.skill_id,2);put(skill.level,1);
    if(periodic)for(auto value:damage)put(std::bit_cast<std::uint32_t>(value),2);
    else {put(captured_hp,2);put(0,2);put(0,2);}
    if(mob.zone)services.send_result(mob,packet,60.0F,6);
}
}
void deliver_mob_applied_skill_periodic_damage(MobRuntime& mob,MobAppliedSkill& skill,MobAppliedSkillDamage& damage,
    MobAppliedSkillDamageServices& services) noexcept {finish_hit(mob,skill,true,0,damage,services);}
void finish_mob_applied_skill_damage(MobRuntime& mob,MobAppliedSkill& skill,const SkillDefinition& definition,
    MobAppliedSkillDamageServices& services) noexcept {
    const bool periodic=definition.detail==0x3d;
    if(!periodic && definition.detail!=0x3f)std::terminate();
    MobAppliedSkillDamage damage{};
    const auto captured_hp=periodic?0U:mob.hp;
    if(periodic) {
        const auto now=services.tick();
        damage=services.time_damage(mob,definition,now-skill.previous_tick);
    }
    finish_hit(mob,skill,periodic,captured_hp,damage,services);
}
}
