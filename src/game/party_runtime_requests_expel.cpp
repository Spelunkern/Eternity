#include "eternity/game/party_runtime_requests_expel.hpp"
#include <stdexcept>
namespace eternity::game {
namespace {
PartyMembershipUser*& slot(PartyRegistryParty& p,std::int32_t i) {
    if(i<0 || i>=30) throw std::invalid_argument("Invalid expulsion slot");
    return i<7?p.slots[static_cast<std::size_t>(i)]:p.extra_slots[static_cast<std::size_t>(i-7)];
}
void refresh(PartyRegistryParty& p) {
    if(p.count<0 || p.count>30) throw std::invalid_argument("Invalid expulsion count");
    std::array<std::optional<std::uint16_t>,30> levels{};
    for(std::int32_t i=0;i<p.count;++i) if(auto* u=slot(p,i)) levels[static_cast<std::size_t>(i)]=u->level;
    p.experience.check_experience_level(std::span(levels).first(static_cast<std::size_t>(p.count)));
}
}
bool expel_party_request_user(PartyRegistryParty& p,const PartyRequestUser& target,PartyRequestWorld& world) {
    auto& w=world.membership_world();auto& user=target.member;
    w.lock(p);
    std::int32_t index=0;
    while(index<p.count && slot(p,index)!=&user) ++index;
    if(index==p.count) {w.unlock(p);return false;}
    user.party=nullptr;
    net::Packet packet{6,0xb};
    for(unsigned i=0;i<4;++i) packet.push_back(static_cast<std::uint8_t>(user.id>>(8*i)));
    w.send_party(p,packet);
    for(auto i=index;i+1<p.count;++i) slot(p,i)=slot(p,i+1);
    --p.count;
    if(p.count==0) w.unregister_party(p);
    else if(p.count==1) {
        auto* selected=slot(p,0);
        auto remaining=world.retain_member(*selected);
        if(&remaining.member!=selected || !remaining.lifetime) throw std::invalid_argument("Invalid retained expulsion member");
        if(p.experience.kind==1) world.leave_raid(p,remaining);
        else leave_normal_party(p,remaining.member,w);
    }
    if(user.guild_creation) w.cleanup_guild_creation(user);
    refresh(p);
    w.unlock(p);
    notify_party_membership_scene(user,w);
    log_party_membership(p,user,0xf02,w);
    return true;
}
void PartyRequestWorld::expel_user(PartyRegistryParty& p,const PartyRequestUser& user) {
    expel_party_request_user(p,user,*this);
}
}
