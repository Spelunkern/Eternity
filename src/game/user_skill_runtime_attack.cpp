#include "eternity/game/user_skill_runtime_attack.hpp"
#include <bit>
#include <stdexcept>
namespace eternity::game {
namespace {
const UserSkillDefinition& begin(UserSkillRuntime& owner,const UserSkillHandle& skill,std::uint32_t now){
    if(!skill||!skill->definition)throw std::invalid_argument("Resolved skill requires retained runtime and definition");
    const auto& info=*skill->definition;const auto seconds=info.reset_seconds;
    skill->reset_tick=now+((owner.actor().status().skills.cool_time>0&&seconds<=500)?0U:std::uint32_t(seconds)*1000U);
    return info; // Original definition remains captured across effect callbacks.
}
void finish(UserSkillRuntime& owner,const UserSkillDefinition& info,UserSkillAttackServices& services){
    if(info.sp_cost||info.mp_cost)services.spend(owner.actor(),info.sp_cost,info.mp_cost);
    if(info.disable_seconds){
        const auto now=services.tick_ms(); // Original rereads WORD after callback.
        owner.actor().status().casting=std::bit_cast<std::int32_t>(now+std::uint32_t(info.disable_seconds)*1000U);
        owner.actor().speed_changed(owner.actor().status());
    }
}
}
void user_skill_runtime_attack_mob(UserSkillRuntime& owner,UserSkillHandle skill,std::uint32_t now,MobRuntime* target,UserSkillAttackServices& services){
    const auto& info=begin(owner,skill,now);
    if(info.effect.effect==1){
        if(info.target_kind==3)services.mob_effect(UserSkillAction::attack_select,owner.actor(),target,info);
        else if(info.target_kind==7)services.mob_effect(UserSkillAction::attack_range,owner.actor(),target,info);
    }else if(info.effect.effect==3){
        if(info.target_kind==3)services.mob_effect(UserSkillAction::debuff_select,owner.actor(),target,info);
        else if(info.target_kind==6)services.mob_effect(UserSkillAction::debuff_around,owner.actor(),nullptr,info);
        else if(info.target_kind==7)services.mob_effect(UserSkillAction::debuff_range,owner.actor(),target,info);
    }
    finish(owner,info,services);
}
void user_skill_runtime_attack_user(UserSkillRuntime& owner,UserSkillHandle skill,std::uint32_t now,Actor* target,UserSkillAttackServices& services){
    const auto& info=begin(owner,skill,now);
    constexpr UserSkillAction actions[]={UserSkillAction::attack,UserSkillAction::heal,UserSkillAction::debuff,UserSkillAction::buff,UserSkillAction::field,UserSkillAction::once};
    if(info.effect.effect>=1&&info.effect.effect<=6)services.user_effect(actions[info.effect.effect-1],owner.actor(),target,info);
    finish(owner,info,services);
}
void UserSkillRuntimeAttackWorld::attack_runtime_mob(Actor& actor,MobRuntime* target,UserSkillHandle skill,std::uint32_t now){
    if(&actor!=&owner_.actor())throw std::invalid_argument("Casting world bound to another Actor");
    user_skill_runtime_attack_mob(owner_,std::move(skill),now,target,services_);
}
void UserSkillRuntimeAttackWorld::attack_runtime_user(Actor& actor,Actor& target,UserSkillHandle skill,std::uint32_t now){
    if(&actor!=&owner_.actor())throw std::invalid_argument("Casting world bound to another Actor");
    user_skill_runtime_attack_user(owner_,std::move(skill),now,&target,services_);
}
}
