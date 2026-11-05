#include "eternity/game/vertical_combat_damage.hpp"
#ifdef __FAST_MATH__
#error AttackDamage requires IEEE nonfinite arithmetic
#endif
#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <stdexcept>
#if defined(__x86_64__) || defined(__i386__)
#include <xmmintrin.h>
#endif
namespace eternity::game {
namespace {
double f32(std::int32_t n){volatile float v=static_cast<float>(n);return v;}
float store(double n){volatile float v=static_cast<float>(n);return v;}
double add(double a,double b){volatile double v=a+b;return v;}
double sub(double a,double b){volatile double v=a-b;return v;}
double mul(double a,double b){volatile double v=a*b;return v;}
double div(double a,double b){volatile double v=a/b;return v;}
std::int32_t integer(double n){n=std::trunc(n);if(!std::isfinite(n)||n<std::numeric_limits<std::int32_t>::min()||n>std::numeric_limits<std::int32_t>::max())return std::numeric_limits<std::int32_t>::min();return static_cast<std::int32_t>(n);}
bool close(ZoneUsers& zone,const Actor& actor,const MobRuntime& mob){
    if(zone.find_actor(actor.id())!=&actor)throw std::invalid_argument("AttackDamage requires the retained zone actor");
    const auto position=zone.position(actor.id());if(!position)throw std::invalid_argument("AttackDamage requires the retained actor position");
    const float dx=store(sub(position->x,mob.position.x)),dz=store(sub(position->z,mob.position.z));
    const float distance=store(std::sqrt(add(mul(dx,dx),mul(dz,dz))));
    return distance<=3.0f;
}
}
namespace {
bool weapon_damage(Actor& actor,MobRuntime& mob,unsigned kind,RandomSource& random,ZoneUsers& zone,VerticalCombatRangedPassive& passive,std::span<const float,81> elements,std::array<std::int32_t,3>& damage,const VerticalCombatSkillDamageInputs* skill){
    if(kind>2)throw std::invalid_argument("AttackDamage supports melee, ranged and magic only");
    if(!mob.definition)throw std::invalid_argument("AttackDamage requires retained mob definition");
    vertical_combat_require_float_environment();
#if defined(__x86_64__) || defined(__i386__)
    // Distance2 consumes float coordinates. FTZ/DAZ would alter denormals.
    if(_mm_getcsr() & 0x8040u)throw std::logic_error("AttackDamage requires SSE denormal handling");
#endif
    if(!skill)damage.fill(0);
    const auto& status=actor.status();const auto& equipment=status.skills.equipment;
    const auto player_level=std::int32_t(status.level),mob_level=std::int32_t(mob.definition->level);
    const auto level_raw=store(mul(div(add(mul(f32(mob_level-player_level-1),100.0),f32(mob_level)),100.0),10.0));
    const double level=std::clamp(level_raw,1.0f,95.0f);
    const auto raw_attack=std::bit_cast<std::int32_t>(std::uint32_t(status.calculated_attack[kind].attack)+std::uint32_t(damage[0]));
    float attack=store(f32(raw_attack));
    if(equipment.variance>0)attack=store(add(attack,f32(random.next()%equipment.variance)));
    if(kind==1 && passive.ranged_penalty_suppression(actor)==0 && close(zone,actor,mob))attack=store(mul(attack,double(0.7f)));
    const auto override_attr=skill?skill->skill.effect.attribute:0;
    const auto attack_attr=override_attr?std::int32_t(override_attr):std::bit_cast<std::int32_t>(equipment.attack_attribute);
    const auto defense_attr=(skill && skill->skill.effect.detail==24)?0:mob.attribute;
    float element=1.0f;
    if(attack_attr<9 && defense_attr<9){
        if(attack_attr<0||defense_attr<0)throw std::out_of_range("AttackDamage negative element index");
        element=elements[std::size_t(attack_attr)*9+std::size_t(defense_attr)];
    }
    const auto wisdom=equipment.base.attributes.wisdom;
    const auto wisdom_roll=[&](){return wisdom==0?0:random.next()%wisdom+1;};
    const auto first_wisdom=wisdom_roll();
    const double defense=f32(mob.attack[kind].defense);
    const auto attack_part=mul(sub(attack,mul(div(attack,100.0),level)),element);
    const auto defense_part=add(defense,mul(div(defense,100.0),level));
    float value=store(add(sub(attack_part,defense_part),mul(f32(first_wisdom),double(0.1f))));
    if(value<0.0f)value=0.0f;
    const double critical_part=mul(div(f32(status.calculated_attack[kind].critical_attack),100.0),100.0);
    const auto critical=std::clamp(integer(sub(critical_part,mul(f32(mob.luck),double(0.034f)))),5,99);
    const bool success=random.next()%100<critical;
    if(success){
        const float bonus=store(mul(f32(wisdom_roll()),double(1.4f)));
        value=store(sub(add(mul(value,1.5),bonus),mul(div(bonus,100.0),level)));
        if(kind==1 && passive.ranged_penalty_suppression(actor)==0 && close(zone,actor,mob))value=store(mul(value,double(0.7f)));
    }
    damage[0]=integer(value);
    if(skill && skill->mode==2){
        for(std::size_t i=0;i<3;++i)if(skill->values[i])damage[i]=std::bit_cast<std::int32_t>(std::uint32_t(damage[i])*std::uint32_t(skill->values[i]));
    }
    for(auto& value:damage)if(value>30000)value=30000;
    if(damage[0]<11)damage[0]=random.next()%10+1;
    return success;
}
} // namespace
bool vertical_combat_normal_damage(Actor& actor,MobRuntime& mob,unsigned kind,RandomSource& random,ZoneUsers& zone,VerticalCombatRangedPassive& passive,std::span<const float,81> elements,std::array<std::int32_t,3>& damage){
    return weapon_damage(actor,mob,kind,random,zone,passive,elements,damage,nullptr);
}
bool vertical_combat_skill_damage(Actor& actor,MobRuntime& mob,unsigned kind,RandomSource& random,ZoneUsers& zone,VerticalCombatRangedPassive& passive,std::span<const float,81> elements,const VerticalCombatSkillDamageInputs& input,std::array<std::int32_t,3>& damage){
    const auto mode=input.mode;
    if(mode==0){for(std::size_t i=0;i<3;++i)damage[i]=input.values[i];return false;}
    if(mode==3){
        const auto& current=actor.resources();
        const std::array<std::uint32_t,3> resources{current.hp,current.sp,current.mp};
        damage.fill(0);
        for(std::size_t i=0;i<3;++i)if(input.values[i]){
            const auto product=std::bit_cast<std::int32_t>(resources[i]*std::uint32_t(input.values[i]));
            damage[i]=std::max(product,1);
        }
        for(auto& value:damage)if(value>30000)value=30000;
        return false;
    }
    if(mode==4||mode==5){
        if(mode==4 && !mob.definition && (input.values[0]||input.values[1]||input.values[2]))throw std::invalid_argument("Skill damage requires retained mob definition");
        if(input.values[0]||input.values[1]||input.values[2]){
            vertical_combat_require_float_environment();
#if defined(__x86_64__) || defined(__i386__)
            if(_mm_getcsr() & 0x8040u)throw std::logic_error("Skill damage requires SSE denormal handling");
#endif
        }
        damage.fill(0);
        for(std::size_t i=0;i<3;++i)if(input.values[i]){
            const std::uint32_t source=mode==4?(i==0?mob.definition->hp:i==1?mob.definition->mp:mob.definition->sp):(i==0?mob.hp:i==1?mob.mp:mob.sp);
            const float stored=store(static_cast<double>(source));
            // _ftol2 0051c424 converts via a signed 64-bit intermediate;
            // AttackDamage consumes its low EAX DWORD, not a 32-bit FISTP.
            const auto product=std::trunc(mul(div(f32(input.values[i]),100.0),stored));
            const auto converted=std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(static_cast<std::int64_t>(product)));
            damage[i]=std::max(converted,1);
        }
        for(auto& value:damage)if(value>30000)value=30000;
        return false;
    }
    if(kind>2)throw std::invalid_argument("Skill damage supports melee, ranged and magic only");
    if(mode==1)for(std::size_t i=0;i<3;++i)damage[i]=input.values[i];else damage.fill(0);
    return weapon_damage(actor,mob,kind,random,zone,passive,elements,damage,&input);
}
bool VerticalCombatNormalDamageWorld::attack_damage(Actor& actor,MobRuntime& mob,unsigned kind,std::array<std::int32_t,3>& damage){return vertical_combat_normal_damage(actor,mob,kind,random_,zone_,passive_,elements_,damage);}
}
