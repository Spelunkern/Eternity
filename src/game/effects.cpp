#include "eternity/game/effects.hpp"
#include <algorithm>
#include <bit>

namespace eternity::game {
namespace {
std::int32_t increment(std::int32_t value) {return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(value)+1);}
std::int32_t decrement(std::int32_t value) {return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(value)-1);}
bool element_buff(const EffectStatus& status,std::uint32_t attribute) {
    if(attribute>8) throw std::invalid_argument("Invalid original attack element state");
    return attribute==0?status.zero_attack_attribute_buff:status.skills.equipment.attack_attribute_buffs[attribute-1]==1;
}
void shape(EffectStatus& status,std::uint8_t value) {status.shape=value;status.skills.equipment.transformed=value!=0;}
void abilities(EffectStatus& status,const SkillDefinition& skill,bool first_negative,bool others_negative,EffectWorld& world) {
    for(std::size_t i=0;i<skill.abilities.size();++i) {
        const auto& ability=skill.abilities[i];std::int32_t value=ability.value;
        if(i==0?first_negative:others_negative) value=-value;
        add_skill_ability(status.skills,skill.effect,ability.type,value);
        if(skill.effect!=3) {
            if(ability.type>=1 && ability.type<=3) world.maximum_changed(status,static_cast<std::uint8_t>(ability.type-1));
            else if(value!=0 && (ability.type==5 || ability.type==7 || ability.type==8)) world.maximum_changed(status,ability.type==5?0:ability.type==7?1:2);
        }
        if(ability.type==16 || ability.type==17) world.speed_changed(status);
    }
}
}
void apply_active_effect(EffectStatus& status,const SkillDefinition& skill,std::uint32_t attacker_id,EffectWorld& world) {
    // CUser::InitApplySkill 00487e50; AddApplySkillBuff 00484cf0 / Debuff 004855b0, ps_game.exe.
    auto& equipment=status.skills.equipment;
    if(skill.effect==3) {
        switch(skill.detail) {
            case 62:case 67:abilities(status,skill,true,true,world);break;
            case 64:status.action_block_a=increment(status.action_block_a);world.speed_changed(status);break;
            case 65:status.movement_block=increment(status.movement_block);world.speed_changed(status);break;
            case 66:status.action_block_b=increment(status.action_block_b);world.speed_changed(status);break;
            case 68:equipment.defense_attribute=0;break;
            case 69:world.taunt(status,skill.aggro,static_cast<float>(skill.apply_range));break;
            case 70:status.chase_id=attacker_id;break;
            case 90:
                shape(status,static_cast<std::uint8_t>(skill.change_type));status.shape_template=skill.change_level;
                if(skill.change_type==101) {status.fixed_form=1;world.speed_changed(status);world.shape_changed(status);}
                else {world.rebuild_attack(status,false);world.shape_changed(status);}
                break;
        }
    } else if(skill.effect==4) {
        switch(skill.detail) {
            case 69:world.taunt(status,skill.aggro,static_cast<float>(skill.apply_range));break;
            case 82:abilities(status,skill,false,false,world);break;
            case 83:
                for(std::size_t i=0;i<3;++i) if(skill.abilities[i].value!=0) {status.defense[i].mirror_skill=skill.id;status.defense[i].mirror_level=skill.level;}
                break;
            case 85:case 86:
                if(skill.defense_type>=3) throw std::invalid_argument("Invalid original defense category");
                status.defense[skill.defense_type].block_mode=skill.detail==85?1:2;status.defense[skill.defense_type].block_value=skill.defense_value;
                break;
            case 87:status.giveup_sp_mp=skill.level;break;
            case 88:shape(status,13);world.shape_changed(status);world.run_mode_changed(status,false);world.stop_attack(status);break;
            case 89:
                status.no_mob_targeting=true;
                if(status.current_target_type==2 && status.current_target!=0 && status.in_zone) world.stop_target_mob(status,status.current_target);
                break;
            case 90:
                shape(status,static_cast<std::uint8_t>(skill.change_type));status.shape_template=status.shape==102?status.form_mob:skill.change_level;
                world.rebuild_attack(status,false);world.shape_changed(status);if(status.shape==100) world.user_shape_changed(status);
                break;
            // Original assembly negates only the first ability for detail 91.
            case 91:abilities(status,skill,true,false,world);break;
            case 92:status.block_all=increment(status.block_all);break;
            case 93:world.detect_hidden(status,skill);break;
            case 94:
                if(skill.attribute<9 && !element_buff(status,skill.attribute)) {
                    if(element_buff(status,equipment.attack_attribute)) world.replace_attack_elements(status,skill.attribute);
                    equipment.attack_attribute=skill.attribute;
                    if(skill.attribute==0) status.zero_attack_attribute_buff=true;else equipment.attack_attribute_buffs[skill.attribute-1]=1;
                }
                break;
            case 95:equipment.defense_attribute=skill.attribute;equipment.defense_attribute_skill=true;break;
        }
    }
}
std::array<std::int32_t,3> periodic_heal(std::uint32_t elapsed,const std::array<std::uint16_t,3>& rates) {
    // RemApplySkillBuff 00485130; CRT _setdefaultprecision 0051136e selects 53-bit arithmetic.
    // Divide by 3000, truncate to qword and retain the low DWORD.
    std::array<std::int32_t,3> amounts{};
    for(std::size_t i=0;i<3;++i) {
        const auto total=static_cast<std::uint64_t>((static_cast<double>(elapsed)/3000.0)*static_cast<double>(rates[i]));
        amounts[i]=std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(total));
    }
    return amounts;
}
void remove_active_effect(EffectStatus& status,const SkillDefinition& skill,const EffectSource& source,std::uint32_t tick,
    EffectEquipment items,const ItemDefinitions& definitions,EffectRemovalWorld& world) {
    // CUser::RemApplySkillBuff 00485130 / RemApplySkillDebuff 00485720, ps_game.exe.
    auto& equipment=status.skills.equipment;
    const auto restore_defense=[&]{if(items.armor) equipment.defense_attribute=item_attribute(*items.armor,definitions);};
    if(skill.effect==0 && skill.detail==6) {
        // CUser::RemApplySkill 00484c60 also expires an activated low-HP passive.
        abilities(status,skill,true,true,world);status.skills.hp_ability_applied=false;
    } else if(skill.effect==3) {
        switch(skill.detail) {
            case 61:case 63:world.finish_periodic_damage(status,skill,source,tick-source.previous_tick);break;
            case 62:case 67:abilities(status,skill,false,false,world);break;
            case 64:status.action_block_a=decrement(status.action_block_a);world.speed_changed(status);break;
            case 65:status.movement_block=decrement(status.movement_block);world.speed_changed(status);break;
            case 66:status.action_block_b=decrement(status.action_block_b);world.speed_changed(status);break;
            case 68:restore_defense();break;
            case 70:status.chase_id=0;break;
            case 90:
                if(status.shape==101) {status.fixed_form=0;world.speed_changed(status);}else world.rebuild_attack(status,true);
                shape(status,0);status.shape_template=0;world.shape_changed(status);break;
        }
    } else if(skill.effect==4) {
        switch(skill.detail) {
            case 81:world.finish_periodic_heal(status,skill,source,periodic_heal(tick-source.previous_tick,skill.time_heal));break;
            case 82:abilities(status,skill,true,true,world);break;
            case 83:
                for(std::size_t i=0;i<3;++i) if(skill.abilities[i].value!=0 && status.defense[i].mirror_skill==skill.id) status.defense[i].mirror_skill=0;
                break;
            case 85:case 86:
                if(skill.defense_type>=3) throw std::invalid_argument("Invalid original defense category");
                status.defense[skill.defense_type].block_mode=0;break;
            case 87:status.giveup_sp_mp=0;break;
            case 88:shape(status,0);world.shape_changed(status);world.run_mode_changed(status,true);break;
            case 89:status.no_mob_targeting=false;break;
            case 90:
                world.rebuild_attack(status,true);shape(status,0);status.shape_template=0;world.shape_changed(status);
                if(skill.change_type==100) world.user_shape_changed(status);
                break;
            case 91:abilities(status,skill,false,true,world);break;
            case 92:status.block_all=std::max(decrement(status.block_all),0);break;
            case 94:
                if(skill.attribute<9) {if(skill.attribute==0) status.zero_attack_attribute_buff=false;else equipment.attack_attribute_buffs[skill.attribute-1]=0;}
                equipment.attack_attribute=items.weapon?item_attribute(*items.weapon,definitions):0;break;
            case 95:restore_defense();equipment.defense_attribute_skill=items.defense_element_remains;break;
        }
    }
}
std::uint8_t attack_speed(const EffectStatus& status) {
    // CUser::GetAttackSpeed 0048a460, ps_game.exe.
    if(status.action_block_b!=0 || status.action_block_a!=0 || status.casting!=0 || status.fixed_form!=0) return 255;
    return static_cast<std::uint8_t>(std::clamp(status.skills.equipment.attack_speed,0,9));
}
std::uint8_t move_speed(const EffectStatus& status) {
    // CUser::GetMoveSpeed 0048a4d0, ps_game.exe.
    if(status.movement_block!=0 || status.action_block_b!=0 || status.action_block_a!=0 || status.movement_restricted!=0 || status.casting!=0) return 255;
    if(status.fixed_form!=0) return 0;
    return static_cast<std::uint8_t>(std::clamp(status.skills.equipment.move_speed,0,4));
}
}
