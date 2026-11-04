#include "eternity/game/mob_applied_skills_effects.hpp"
#include <bit>

namespace eternity::game {
namespace {
void add(std::int32_t& value,std::int32_t delta) noexcept {
    value=std::bit_cast<std::int32_t>(std::bit_cast<std::uint32_t>(value)+std::bit_cast<std::uint32_t>(delta));
}
}
void set_mob_applied_skill_ability(MobRuntime& mob,std::uint32_t type,std::int32_t delta,
                                   MobAppliedSkillEffectServices& services) noexcept {
    switch(type) {
    case 7:add(mob.wisdom,delta);break;
    case 8:add(mob.dexterity,delta);break;
    case 9:add(mob.luck,delta);break;
    case 0x10:add(mob.attack_speed,delta);services.send_speed(mob);return;
    case 0x11:add(mob.move_speed,delta);services.send_speed(mob);return;
    default:
        if(type>=0x14 && type<=0x1f) {
            const auto index=static_cast<std::size_t>((type-0x14)%3);
            auto& category=mob.additions[index];
            // Native fields project original stAttackAdd offsets +0,+4,+8,+12.
            switch((type-0x14)/3) {
            case 0:add(category.accuracy,delta);break;
            case 1:add(category.attack,delta);break;
            case 2:add(category.defense,delta);break;
            case 3:add(category.evasion,delta);break;
            }
        } else {
            if(type>=0x20 && type<=0x22)add(mob.attack_disabled[type-0x20],delta);
            return;
        }
    }
    services.rebuild_attack(mob);
}
void remove_mob_applied_skill_effects(MobRuntime& mob,MobAppliedSkill& skill,
                                     const SkillDefinition& definition,std::uint8_t base_attribute,
                                     MobAppliedSkillEffectServices& services) noexcept {
    switch(definition.detail) {
    case 0x3d:case 0x3f:services.finish_damage_skill(mob,skill,definition);return;
    case 0x3e:case 0x43:
        for(const auto& ability:definition.abilities)
            set_mob_applied_skill_ability(mob,ability.type,ability.value,services);
        return;
    case 0x40:add(mob.stun,-1);services.send_speed(mob);return;
    case 0x41:add(mob.stop,-1);services.send_speed(mob);return;
    case 0x42:add(mob.sleep,-1);services.send_speed(mob);return;
    case 0x44:mob.attribute=base_attribute;return;
    case 0x48:mob.vision_fixed_user=0;return;
    default:return;
    }
}
}
