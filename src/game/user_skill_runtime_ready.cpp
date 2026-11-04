#include "eternity/game/user_skill_runtime_ready.hpp"
#include <stdexcept>
namespace eternity::game {
namespace {
bool error(UserSkillRuntime& owner,const UserSkillRuntimeSkill& skill,std::uint8_t reason,bool mob){
    net::Packet packet(20,0);const auto put=[&](unsigned at,std::uint32_t v,unsigned n){for(unsigned i=0;i<n;++i)packet[at+i]=static_cast<std::uint8_t>(v>>(8*i));};
    put(0,mob?0x517:0x511,2);packet[2]=reason;put(3,owner.actor().id(),4);put(11,skill.skill_id,2);packet[13]=static_cast<std::uint8_t>(skill.level);owner.actor().send_self(packet);return false;
}
bool common(UserSkillRuntime& owner,const UserSkillHandle& skill,const UserSkillDefinition& info,std::uint32_t now,bool mob,UserSkillReadyWorld& world){
    if(!skill)throw std::invalid_argument("Skill readiness requires retained actual runtime");
    // Positive cool-time reduction wraps as DWORD*1000. Reduction >= reset
    // bypasses the time comparison; a negative reduction is ignored.
    auto deadline=skill->reset_tick;const auto reduction=owner.actor().status().skills.cool_time;
    if(reduction>0){const auto milliseconds=static_cast<std::uint32_t>(reduction)*1000U;if(milliseconds>=deadline)deadline=0;else deadline-=milliseconds;}
    if(deadline>now)return error(owner,*skill,9,mob);
    if(info.required_skill){
        std::uint32_t index=0;bool found=false;
        for(std::int32_t i=0;i<owner.learned_count();++i){auto p=owner.slot(static_cast<std::uint32_t>(i));if(p&&p->skill_id==info.required_skill){index=static_cast<std::uint32_t>(i);found=true;break;}}
        if(!found)return error(owner,*skill,8,false); // Mob branch also sends0511.
        const auto fresh=world.tick_ms();const auto prerequisite=owner.slot(index);
        if(!prerequisite)throw std::logic_error("Required skill disappeared during clock callback");
        if(prerequisite->reset_tick<=fresh)return error(owner,*skill,8,false);
    }
    if(info.attack_kind>=owner.actor().status().skills.attack_disabled.size())throw std::out_of_range("Skill attack kind outside actual three status groups");
    // SetSkillAbility 00486050/5a/64: +13a4/+13d0/+13fc, 44-byte stride.
    if(owner.actor().status().skills.attack_disabled[info.attack_kind])return error(owner,*skill,10,false);
    return true;
}
}
bool user_skill_runtime_ready_user_after_equipment(UserSkillRuntime& owner,UserSkillHandle skill,const UserSkillDefinition& info,std::uint32_t now,Actor* target,UserSkillReadyWorld& world){
    if(!common(owner,skill,info,now,false,world))return false;
    if(target&&!world.in_user_range(owner.actor(),*target,info.attack_range))return error(owner,*skill,5,false);
    return true;
}
bool user_skill_runtime_ready_mob_after_equipment(UserSkillRuntime& owner,UserSkillHandle skill,const UserSkillDefinition& info,std::uint32_t now,MobRuntime* target,UserSkillReadyWorld& world){
    if(!common(owner,skill,info,now,true,world))return false;
    if(target&&!world.in_mob_range(owner.actor(),*target,info.attack_range))return error(owner,*skill,5,true);
    return true;
}
}
