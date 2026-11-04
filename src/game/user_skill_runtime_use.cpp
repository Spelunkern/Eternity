#include "eternity/game/user_skill_runtime_use.hpp"
#include <algorithm>
#include <stdexcept>
namespace eternity::game {
namespace {
void idle(Actor& actor){auto& a=actor.attack_state();if(a.mode!=0&&a.mode!=1){if(a.recovery<1)a.recovery=1;a.mode=0;}}
void resume(Actor& actor){auto& a=actor.attack_state();if(a.mode!=1&&a.mode!=2){if(a.mode==0){actor.send_self(net::Packet{0x12,2});if(a.recovery<2)a.recovery=2;}a.mode=2;}a.next_tick=1;}
void rejected(Actor& actor,bool mob){if(!actor.attack_state().previous_tick)idle(actor);else if(mob)resume(actor);else actor.attack_state().next_tick=1;}
void instant_tail(Actor& actor,std::uint32_t now){if(!actor.attack_state().previous_tick)idle(actor);else resume(actor);constexpr std::uint32_t delays[]={4000,3000,2750,2500,2250,2000,1750,1500,1250,1000};actor.attack_state().resume_tick=now+delays[std::clamp(actor.status().skills.equipment.attack_speed,0,9)];}
net::Packet casting(Actor& actor,const UserSkillRuntimeSkill& skill,std::uint32_t target,bool mob){net::Packet p(13,0);const auto put=[&](unsigned at,std::uint32_t v,unsigned n){for(unsigned i=0;i<n;++i)p[at+i]=static_cast<std::uint8_t>(v>>(8*i));};put(0,mob?0x516:0x510,2);put(2,actor.id(),4);put(6,target,4);put(10,skill.skill_id,2);p[12]=static_cast<std::uint8_t>(skill.level);return p;}
}
void user_skill_runtime_use_user(UserSkillRuntime& owner,UserSkillHandle skill,std::uint32_t now,Actor* target,UserSkillUseServices& services){
    if(!skill){throw std::invalid_argument("Skill preparation requires actual retained runtime");}auto& actor=owner.actor();
    if(!services.eligible_user(owner,target,skill,now)){rejected(actor,false);return;}
    if(!skill->definition){throw std::logic_error("Validated runtime lost definition");}const auto& info=*skill->definition;
    if(info.ready_time){
        if(!target)throw std::invalid_argument("Ready user skill requires actual target");
        if(actor.attack_state().mode==2)actor.attack_state().mode=3;
        const auto packet=casting(actor,*skill,target->id(),false);
        if(actor.status().in_zone)services.cast_user(actor,*target,info,packet);
        actor.attack_state().resume_tick=now+std::uint32_t(info.ready_time)*250U;
    }else{user_skill_runtime_attack_user(owner,std::move(skill),now,target,services);instant_tail(actor,now);}
}
void user_skill_runtime_use_mob(UserSkillRuntime& owner,UserSkillHandle skill,std::uint32_t now,MobRuntime* target,UserSkillUseServices& services){
    if(!skill){throw std::invalid_argument("Skill preparation requires actual retained runtime");}auto& actor=owner.actor();
    if(!services.eligible_mob(owner,target,skill,now)){rejected(actor,true);return;}
    if(!skill->definition){throw std::logic_error("Validated runtime lost definition");}const auto& info=*skill->definition;
    if(info.ready_time){
        if(!target)throw std::invalid_argument("Ready mob skill requires actual target");
        if(actor.attack_state().mode==2)actor.attack_state().mode=3;
        const auto packet=casting(actor,*skill,target->id,true);
        if(actor.status().in_zone)services.cast_mob(actor,*target,info,packet);
        actor.attack_state().resume_tick=now+std::uint32_t(info.ready_time)*250U;
    }else{user_skill_runtime_attack_mob(owner,std::move(skill),now,target,services);instant_tail(actor,now);}
}
void UserSkillRuntimeUseWorld::use_runtime_user(Actor& actor,Actor& target,UserSkillHandle skill,std::uint32_t now){if(&actor!=&owner_.actor())throw std::invalid_argument("Preparation world bound to another Actor");user_skill_runtime_use_user(owner_,std::move(skill),now,&target,services_);}
void UserSkillRuntimeUseWorld::use_runtime_mob(Actor& actor,MobRuntime* target,UserSkillHandle skill,std::uint32_t now){if(&actor!=&owner_.actor())throw std::invalid_argument("Preparation world bound to another Actor");user_skill_runtime_use_mob(owner_,std::move(skill),now,target,services_);}
}
