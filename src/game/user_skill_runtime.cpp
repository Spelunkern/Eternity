#include "eternity/game/user_skill_runtime.hpp"
#include <bit>
#include <stdexcept>
namespace eternity::game {
UserSkillRuntime::UserSkillRuntime(std::shared_ptr<Actor> actor,UserSkillRuntimeServices& services):actor_(std::move(actor)),services_(services){if(!actor_)throw std::invalid_argument("User skill runtime requires retained Actor ownership");}
UserSkillRuntime::~UserSkillRuntime(){
    // ps_game.exe ~CUser 004496a0: only indices [0,signed count) are deleted.
    for(std::size_t i=0;i<slots_.size();++i)if(slots_[i]){
        if(count_>0 && i<static_cast<std::uint32_t>(count_))services_.destroy(std::move(slots_[i]));
        else services_.strand(std::move(slots_[i]));
    }
}
UserSkillLoadResult UserSkillRuntime::load(std::span<const std::uint8_t> packet,std::uint32_t tick){
    if(packet.size()<7 || packet[0]!=4 || packet[1]!=5 || packet.size()<7+std::size_t(packet[6])*6)throw std::invalid_argument("Truncated/non-0504 learned skill packet");
    const auto word=[&](std::size_t at){return static_cast<std::uint16_t>(packet[at]|(unsigned(packet[at+1])<<8));};
    // PacketUserDBSetInfo 0046d780, 0504 branch 0046e166..0046e216.
    for(unsigned i=0;i<packet[6];++i){
        const auto at=7+i*6;const auto id=word(at);const auto level=packet[at+2],number=packet[at+3];
        const auto* info=services_.definition(id,level);
        if(!info){services_.invalid_definition(id,level);services_.disconnect(0x68,0);return UserSkillLoadResult::disconnect_requested;}
        if(id==0){services_.disconnect(0x68,0);return UserSkillLoadResult::disconnect_requested;} // Lookup precedes this check in the original.
        auto item=services_.allocate();if(!item)continue; // Failure skips this row.
        item->definition=info;item->skill_id=id;item->level=level;item->number=number;
        item->reset_tick=tick+std::uint32_t(word(at+4))*1000U;
        if(slots_[number])services_.strand(std::move(slots_[number]));
        slots_[number]=std::move(item);
        count_=std::bit_cast<std::int32_t>(std::bit_cast<std::uint32_t>(count_)+1U);
    }
    return UserSkillLoadResult::loaded;
}
UserSkillHandle UserSkillRuntime::slot(std::uint32_t n)const{if(n>=slots_.size())throw std::out_of_range("User skill slot outside original 256 entries");return slots_[n];}
UserSkillHandle UserSkillRuntime::selected()const{return slot(selection_);}
UserSkillHandle UserSkillRuntime::request_slot(std::uint8_t n)const{return std::int32_t(n)<count_?slots_[n]:nullptr;}
void UserSkillRuntime::reset_character()noexcept{count_=0;selection_=0;}
const MobCombatSchedulerSkillBinding* UserSkillRuntimeWorld::selected_skill(const Actor& actor)const{
    if(&actor!=&owner_.actor())throw std::invalid_argument("User skill world bound to another Actor");
    auto runtime=owner_.selected();if(!runtime)return nullptr;
    if(!runtime->definition)throw std::logic_error("Learned runtime without retained definition");
    runtime->target_type=runtime->definition->effect.detail;runtime->name=runtime->definition->name;
    return runtime.get();
}
UserSkillHandle UserSkillRuntimeWorld::capture(Actor& actor,const MobCombatSchedulerSkillBinding& binding)const{
    if(&actor!=&owner_.actor())throw std::invalid_argument("User skill world bound to another Actor");
    const auto& runtime=static_cast<const UserSkillRuntimeSkill&>(binding);
    return std::const_pointer_cast<UserSkillRuntimeSkill>(runtime.shared_from_this());
}
void UserSkillRuntimeWorld::use_skill_mob(Actor& a,MobRuntime* m,const MobCombatSchedulerSkillBinding& s,std::uint32_t t){use_runtime_mob(a,m,capture(a,s),t);}
void UserSkillRuntimeWorld::attack_skill_mob(Actor& a,MobRuntime* m,const MobCombatSchedulerSkillBinding& s,std::uint32_t t){attack_runtime_mob(a,m,capture(a,s),t);}
void UserSkillRuntimeWorld::use_skill_user(Actor& a,Actor& u,const MobCombatSchedulerSkillBinding& s,std::uint32_t t){use_runtime_user(a,u,capture(a,s),t);}
void UserSkillRuntimeWorld::attack_skill_user(Actor& a,Actor& u,const MobCombatSchedulerSkillBinding& s,std::uint32_t t){attack_runtime_user(a,u,capture(a,s),t);}
}
