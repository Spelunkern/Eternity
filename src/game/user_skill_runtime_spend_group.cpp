#include "eternity/game/user_skill_runtime_spend_group.hpp"
#include <stdexcept>
namespace eternity::game {
void user_skill_runtime_spend_group(Actor& actor,const UserSkillSpendTransition& debit,
    UserSkillSpendGroupWorld& world){
    auto locked=world.membership(actor);
    struct Guard {
        UserSkillSpendGroupLease group;
        ~Guard(){if(group)group->unlock();}
    } guard;
    if(locked){
        locked->lock();guard.group=locked;
        if(!world.membership(actor)){guard.group.reset();locked->unlock();locked.reset();}
    }
    auto live=world.membership(actor);
    if(!live)return;
    const auto kind=live->kind();
    if(kind>1)throw std::logic_error("SendUseSPMP group kind has undefined original packet opcode");
    net::Packet packet(9,0);packet[0]=kind==0?0x01:0x0a;packet[1]=0x0c;
    const auto id=actor.id();for(unsigned i=0;i<4;++i)packet[2+i]=static_cast<std::uint8_t>(id>>(8*i));
    const auto send=[&](UserSkillSpendGroup& group,std::uint8_t tag,std::uint32_t value){
        packet[6]=tag;packet[7]=static_cast<std::uint8_t>(value);packet[8]=static_cast<std::uint8_t>(value>>8);
        world.send(group,actor,packet);
    };
    if(debit.requested_sp>0)send(*live,1,actor.resources().sp);
    if(debit.requested_mp>0){
        const auto mp=actor.resources().mp;
        live=world.membership(actor);
        if(!live)throw std::logic_error("SendUseSPMP lost group before MP dispatch");
        send(*live,2,mp);
    }
}
}
