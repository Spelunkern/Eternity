#include "eternity/game/vertical_combat_success.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <stdexcept>
namespace eternity::game {
namespace {
std::int32_t sum(std::int32_t a,std::int32_t b){return std::bit_cast<std::int32_t>(std::uint32_t(a)+std::uint32_t(b));}
double spill(std::int32_t n){return static_cast<float>(n);}
std::int32_t integer(double n){n=std::trunc(n);if(!std::isfinite(n)||n<std::numeric_limits<std::int32_t>::min()||n>std::numeric_limits<std::int32_t>::max())return std::numeric_limits<std::int32_t>::min();return static_cast<std::int32_t>(n);}
}
bool vertical_combat_normal_success(const Actor& actor,const MobRuntime& mob,unsigned kind,RandomSource& random){
    if(mob.unbeatable)return false;
    const auto status=mob.state_stack?(mob.state_stack->empty()?0:mob.state_stack->back()):mob.status;
    if(status==3)return false;
    if(kind>2)return true;
    if(!mob.definition)throw std::invalid_argument("AttackSuccess requires retained mob definition");
    const auto& user=actor.status().calculated_attack[kind];const auto& enemy=mob.attack[kind];
    if(kind==2){
        const auto player_level=std::int32_t(actor.status().level),mob_level=std::int32_t(mob.definition->level);
        const double denominator=static_cast<float>(player_level+mob_level);
        const double level_term=static_cast<float>((((double(static_cast<float>(mob_level-player_level-2))*100.0)+double(static_cast<float>(mob_level)))/denominator)*double(1.1F)+spill(enemy.evasion));
        const auto dex=actor.status().skills.equipment.base.attributes.dexterity;
        const auto enemy_dex=std::int32_t(mob.definition->dexterity);
        const auto numerator=std::bit_cast<std::int32_t>(std::uint32_t(enemy_dex)*11u-std::uint32_t(dex)*10u);
        const double dex_term=static_cast<float>((spill(numerator)/spill(sum(dex,enemy_dex)))*double(3.9F)+spill(user.accuracy));
        const auto clamp_float=[](double v){return v<1.0?1.0:v>70.0?70.0:v;};
        const auto chance=std::clamp(integer(clamp_float(level_term)+clamp_float(dex_term)),1,90);
        return random.next()%100>=chance;
    }
    const double level=static_cast<float>(actor.status().level);
    const double levels=static_cast<float>(std::uint32_t(actor.status().level)+mob.definition->level);
    const double level_part=(level/levels)*160.0;
    const double defense=(spill(enemy.accuracy)/spill(sum(user.evasion,enemy.accuracy)))*100.0;
    const double accuracy=(spill(user.accuracy)/spill(sum(enemy.evasion,user.accuracy)))*100.0;
    const auto chance=std::clamp(integer(level_part-(defense-accuracy)),20,99);
    return random.next()%100<chance;
}
bool vertical_combat_skill_success(const Actor& actor,const MobRuntime& mob,unsigned kind,const VerticalCombatSkillSuccessInputs& input,RandomSource& random){
    if(mob.unbeatable)return false;
    const auto status=mob.state_stack?(mob.state_stack->empty()?0:mob.state_stack->back()):mob.status;
    if(status==3)return false;
    if(input.skill.state_type){
        if(input.state_immunity.size()<input.skill.state_type)throw std::invalid_argument("Missing authoritative mob state immunity bytes");
        if(input.state_immunity[input.skill.state_type-1])return false;
    }
    if(input.skill_immunity.size()!=6)throw std::invalid_argument("Missing six authoritative mob skill immunity bytes");
    for(auto id:input.skill_immunity)if(id==input.skill.effect.id)return false;
    if(input.hit_type>=1&&input.hit_type<=3){
        auto chance=std::int32_t(input.hit_value);
        if(input.hit_type!=1){
            if(!mob.definition)throw std::invalid_argument("AttackSuccess requires retained mob definition");
            const auto& attributes=actor.status().skills.equipment.base.attributes;
            const auto attribute=input.hit_type==2?attributes.wisdom:attributes.dexterity;
            const auto enemy=input.hit_type==2?mob.definition->wisdom:mob.definition->dexterity;
            const auto divisor=sum(attribute,enemy);
            if(divisor==0 || (attribute==std::numeric_limits<std::int32_t>::min()&&divisor==-1))throw std::domain_error("Original skill hit division fault");
            chance=sum(chance,std::bit_cast<std::int32_t>(std::uint32_t(attribute/divisor)*100u));
        }
        return random.next()%100<chance;
    }
    return vertical_combat_normal_success(actor,mob,kind,random);
}
bool VerticalCombatNormalSuccessWorld::attack_success(Actor& actor,MobRuntime& mob,unsigned kind){return vertical_combat_normal_success(actor,mob,kind,random_);}
}
