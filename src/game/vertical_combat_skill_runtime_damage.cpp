#include "eternity/game/vertical_combat_skill_runtime_damage.hpp"
#include <stdexcept>
namespace eternity::game {
VerticalCombatSkillRuntimeDamage::VerticalCombatSkillRuntimeDamage(UserSkillRuntime& owner,RandomSource& random,ZoneUsers& zone,VerticalCombatRangedPassive& passive,std::span<const float,81> elements,std::span<const std::int32_t> recovery)
    :owner_(owner),random_(random),zone_(zone),passive_(passive),elements_(elements),recovery_(recovery){}
bool VerticalCombatSkillRuntimeDamage::calculate(Actor& actor,MobRuntime& mob,const UserSkillDefinition& captured,std::uint8_t mode,std::array<std::uint16_t,3> values,std::array<std::int32_t,3>& damage){
    if(&actor!=&owner_.actor())throw std::invalid_argument("Skill damage runtime bound to another actor");
    return vertical_combat_skill_damage(actor,mob,captured.attack_kind,random_,zone_,passive_,elements_,{captured,mode,values,recovery_},damage);
}
}
