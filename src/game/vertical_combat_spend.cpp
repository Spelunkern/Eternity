#include "eternity/game/vertical_combat_spend.hpp"
#include <stdexcept>
#include <bit>
#include <cmath>
#include <limits>
namespace eternity::game {
namespace {
double recovery_float(std::int32_t value){volatile float result=static_cast<float>(value);return result;}
double recovery_multiply(double a,double b){volatile double result=a*b;return result;}
double recovery_add(double a,double b){volatile double result=a+b;return result;}
std::int32_t standing_amount(std::uint32_t maximum,std::int32_t percent,std::int32_t flat){
 const auto max=recovery_float(std::bit_cast<std::int32_t>(maximum));
 const auto rate=recovery_float(percent);
 const auto total=recovery_add(recovery_add(recovery_multiply(recovery_multiply(rate,max),double(0.01f)),recovery_multiply(max,double(0.05f))),recovery_float(flat));
 const auto truncated=std::trunc(total);
 if(!std::isfinite(truncated)||truncated<std::numeric_limits<std::int32_t>::min()||truncated>std::numeric_limits<std::int32_t>::max())return std::numeric_limits<std::int32_t>::min();
 return static_cast<std::int32_t>(truncated);
}
}
void vertical_combat_replace_recovery_blessing(LoadedActor& loaded,const BlessingDefinition& definition){
 if(!loaded.actor)throw std::invalid_argument("Blessing recovery requires the retained actor");
 if(definition.modifiers[13]||definition.modifiers[14]||definition.modifiers[15])vertical_combat_require_float_environment();
 const auto& maximum=loaded.actor->status().skills.equipment.base.maximum;
 const std::array<std::uint32_t,3> resources{maximum.hp,maximum.sp,maximum.mp};
 for(std::size_t i=0;i<3;++i){
  const auto old=loaded.blessing[i+13];const auto rate=definition.modifiers[i+13];
  const auto product=std::bit_cast<std::int32_t>(resources[i]*static_cast<std::uint32_t>(rate));
  const auto value=rate==0?0:static_cast<std::int32_t>(std::trunc(recovery_multiply(recovery_float(product),double(0.01f))));
  loaded.attack_recovery[i]=std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(loaded.attack_recovery[i])-static_cast<std::uint32_t>(old)+static_cast<std::uint32_t>(value));
  loaded.blessing[i+13]=value;
 }
}
std::array<std::int32_t,3> vertical_combat_standing_recovery_amounts(const Actor& actor){
 vertical_combat_require_float_environment();
 const auto& skills=actor.status().skills;const auto& maximum=skills.equipment.base.maximum;
 return {standing_amount(maximum.hp,skills.sitting_recovery_percent[0],skills.equipment.standing_recovery[0]),standing_amount(maximum.sp,skills.sitting_recovery_percent[1],skills.equipment.standing_recovery[1]),standing_amount(maximum.mp,skills.sitting_recovery_percent[2],skills.equipment.standing_recovery[2])};
}
VerticalCombatRecoveryResult vertical_combat_recovery(LoadedActor& loaded,std::uint32_t now,std::uint32_t& next_tick){
 if(!loaded.actor)throw std::invalid_argument("Recovery requires the retained actor");
 auto& actor=*loaded.actor;auto& state=actor.attack_state();
 if(now<next_tick || state.mode==1)return VerticalCombatRecoveryResult::skipped;
 std::array<std::int32_t,3> amounts;
 if(state.recovery==0){
  amounts=vertical_combat_standing_recovery_amounts(actor);
 }else amounts=state.recovery==1?actor.status().skills.equipment.sitting_recovery:loaded.attack_recovery;
 actor.recover(amounts,true,actor.presentation_motion()==1);
 if(state.mode==0){if(actor.presentation_motion()==1)state.recovery=0;else if(actor.presentation_motion()==0)state.recovery=1;}
 else if(state.mode==2)state.recovery=2;
 next_tick=now+5000u;
 return VerticalCombatRecoveryResult::applied;
}
void vertical_combat_spend(Actor& actor,std::uint16_t sp,std::uint16_t mp,UserSkillSpendGroupWorld& group){
 auto resources=actor.resources();
 const auto& benefits=actor.status().skills;
 const auto debit=user_skill_runtime_spend_transition(resources.sp,resources.mp,sp,mp,benefits.sp_cost_reduction,benefits.mp_cost_reduction);
 resources.sp=debit.sp;resources.mp=debit.mp;
 actor.replace_resources_raw(resources);
 actor.send_self(debit.self_packet);
 user_skill_runtime_spend_group(actor,debit,group);
}
VerticalCombatSpendSkillServices::VerticalCombatSpendSkillServices(RandomSource& r,VerticalCombatSkillRuntimeDamage& d,MobTargetingZone& t,const ItemDefinitions& i,VerticalCombatView& v,VerticalCombatHitServices& h,VerticalCombatSkillSlotServices& s,UserSkillSpendGroupWorld& g):VerticalCombatAppliedSkillServices(r,d,t,i,v,h,s),group_(g){}
void VerticalCombatSpendSkillServices::spend(Actor& actor,std::uint16_t sp,std::uint16_t mp){vertical_combat_spend(actor,sp,mp,group_);}
}
