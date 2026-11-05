#include "eternity/game/vertical_combat_scheduler.hpp"
#include <stdexcept>
namespace eternity::game {
void VerticalCombatSchedulerUseServices::user_effect(UserSkillAction a,Actor& actor,Actor* target,const UserSkillDefinition& info){actions_.user_effect(a,actor,target,info);}
void VerticalCombatSchedulerUseServices::mob_effect(UserSkillAction a,Actor& actor,MobRuntime* target,const UserSkillDefinition& info){actions_.mob_effect(a,actor,target,info);}
void VerticalCombatSchedulerUseServices::spend(Actor& actor,std::uint16_t sp,std::uint16_t mp){actions_.spend(actor,sp,mp);}
std::uint32_t VerticalCombatSchedulerUseServices::tick_ms(){return actions_.tick_ms();}
void VerticalCombatSchedulerWorld::attack_mob(Actor& actor,MobRuntime& mob){
 if(&actor!=&owner_.actor())throw std::invalid_argument("Combat scheduler bound to another Actor");
 vertical_combat_attack_mob(actor,mob,normal_);
}
}
