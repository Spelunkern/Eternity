#include "eternity/game/user_skill_runtime_equipment.hpp"
#include <stdexcept>
namespace eternity::game {
namespace {
std::uint8_t permission(const UserSkillDefinition& info,std::uint8_t type){
    // Original reads stSkillInfo[+33 + real_type], including adjacent fields.
    if(type==0)return static_cast<std::uint8_t>(info.effect.detail>>8);
    if(type<=16)return info.need_weapon[type-1];
    switch(type){
        case 17:return static_cast<std::uint8_t>(info.sp_cost);case 18:return static_cast<std::uint8_t>(info.sp_cost>>8);
        case 19:return static_cast<std::uint8_t>(info.mp_cost);case 20:return static_cast<std::uint8_t>(info.mp_cost>>8);
        case 21:return info.ready_time;
        case 22:if(!info.weapon_padding)throw std::logic_error("Original definition padding byte +49 is unavailable");return *info.weapon_padding;
        case 23:return static_cast<std::uint8_t>(info.reset_seconds);case 24:return static_cast<std::uint8_t>(info.reset_seconds>>8);
        case 25:return info.attack_range;case 26:return info.state_type;case 27:return info.effect.attribute;
        default:throw std::out_of_range("Item real type outside original GetItemRType domain");
    }
}
const UserSkillDefinition& definition(const UserSkillHandle& skill){if(!skill||!skill->definition)throw std::invalid_argument("Eligibility requires actual runtime/definition");return *skill->definition;}
}
bool user_skill_runtime_check_equipment(UserSkillRuntime& owner,UserSkillHandle skill,const UserSkillDefinition& info,bool mob,UserSkillEquipmentWorld& world){
    if(!skill){throw std::invalid_argument("Equipment validation requires retained runtime");}bool allowed=false;
    if(info.need_weapon[15])allowed=world.equipped(owner.actor(),6)!=nullptr;
    else if(const auto* item=world.equipped(owner.actor(),5)){
        if(!item->definition)throw std::logic_error("Retained weapon lacks actual item definition");
        allowed=permission(info,item->definition->real_type)!=0;
    }
    if(allowed)return true;
    net::Packet p(20,0);const auto put=[&](unsigned at,std::uint32_t v,unsigned n){for(unsigned i=0;i<n;++i)p[at+i]=static_cast<std::uint8_t>(v>>(8*i));};put(0,mob?0x517:0x511,2);p[2]=7;put(3,owner.actor().id(),4);put(11,skill->skill_id,2);p[13]=static_cast<std::uint8_t>(skill->level);owner.actor().send_self(p);return false;
}
bool UserSkillRuntimeEligibilityServices::eligible_user(UserSkillRuntime& owner,Actor* target,UserSkillHandle skill,std::uint32_t now){const auto& info=definition(skill);return user_skill_runtime_check_resources(owner,skill,info,false)&&user_skill_runtime_check_equipment(owner,skill,info,false,*this)&&user_skill_runtime_ready_user_after_equipment(owner,std::move(skill),info,now,target,*this);}
bool UserSkillRuntimeEligibilityServices::eligible_mob(UserSkillRuntime& owner,MobRuntime* target,UserSkillHandle skill,std::uint32_t now){const auto& info=definition(skill);return user_skill_runtime_check_resources(owner,skill,info,true)&&user_skill_runtime_check_equipment(owner,skill,info,true,*this)&&user_skill_runtime_ready_mob_after_equipment(owner,std::move(skill),info,now,target,*this);}
}
