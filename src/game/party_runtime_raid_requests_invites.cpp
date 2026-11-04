#include "eternity/game/party_runtime_raid_requests_invites.hpp"
#include "eternity/game/party_runtime_raid_requests_autojoin.hpp"
#include "eternity/game/party_runtime_raid_requests_lock.hpp"
#include <stdexcept>
namespace eternity::game {
namespace {
PartyRegistryParty* party(const PartyRequestUser& u){return static_cast<PartyRegistryParty*>(u.member.party);}
std::uint32_t read(const net::Packet& b,std::size_t at,unsigned bytes=4){std::uint32_t n=0;for(unsigned i=0;i<bytes;++i)n|=static_cast<std::uint32_t>(b.at(at+i))<<(8*i);return n;}
void append(net::Packet& b,std::uint32_t n,unsigned bytes=4){for(unsigned i=0;i<bytes;++i)b.push_back(static_cast<std::uint8_t>(n>>(8*i)));}
void retained(const PartyRequestUser& u){if(!u.lifetime)throw std::logic_error("Raid invitation lookup returned unretained user");}
PartyRegistryParty& required_party(const PartyRequestUser& u){auto* p=party(u);if(!p)throw std::logic_error("Raid role callback invalidated required live party");return *p;}
bool may_invite(const PartyRequestUser& u,PartyMembershipWorld& w){
    auto* p=party(u);if(!p)return true;
    if(is_party_request_leader(*p,u.member,w) && required_party(u).count<30)return true;
    p=&required_party(u);
    return is_raid_request_sub_leader(*p,u.member,w) && required_party(u).count<30;
}
bool enter(PartyRegistryParty& p,const PartyRequestUser& u,PartyRaidRequestsWorld& world){return p.experience.kind==1?world.enter_raid(p,u):enter_normal_party(p,u.member,world.membership_world());}
}
bool is_raid_request_sub_leader(PartyRegistryParty& p,PartyMembershipUser& u,PartyMembershipWorld& w){
    if(p.experience.kind==0)return false;
    w.lock(p);const bool result=p.count>0 && raid_membership_slot(p,p.sub_leader)==&u;w.unlock(p);return result;
}
bool dispatch_raid_invitation_request(const PartyRequestUser& u,net::Packet& packet,PartyRaidRequestsWorld& world){
    const auto op=read(packet,0,2);if(op!=0xb14 && op!=0xb15 && op!=0xb18)return false;
    auto& w=world.membership_world();
    if(op==0xb15){
        if(u.state.inviter_id==0 || u.member.party)return true;
        auto inviter=world.find_zone_user(u,u.state.inviter_id);
        u.state.inviter_id=0;u.state.invitation_deadline=0;
        if(!inviter)return true;
        retained(*inviter);
        if(inviter->state.activity!=0 || u.state.activity!=0){send_raid_request_error(*inviter,1,u.member.id,w);return true;}
        if(packet.at(2)!=0){send_raid_request_error(*inviter,0,u.member.id,w);return true;}
        bool joined=false;
        {
            PartyRaidRequestLock lock(*inviter,w);auto* p=party(*inviter);
            if(!p){p=create_registry_party(world.registry(),&inviter->member,1,world.registry_ports(),w);if(p)joined=enter(*p,u,world);}
            else if(is_party_request_leader(*p,inviter->member,w) || is_raid_request_sub_leader(*p,inviter->member,w))joined=enter(*p,u,world);
        }
        if(!joined)send_raid_request_error(*inviter,1,u.member.id,w);
        return true;
    }
    if(op==0xb14){
        const auto id=read(packet,2);
        if(id==u.member.id){send_raid_request_error(u,4,id,w);return true;}
        if(u.state.activity!=0){send_raid_request_error(u,1,id,w);return true;}
        PartyRaidRequestLock lock(u,w);
        if(!may_invite(u,w)){send_raid_request_error(u,2,id,w);return true;}
        auto target=world.find_zone_user(u,id);
        if(!target){send_raid_request_error(u,4,id,w);return true;}retained(*target);
        if(target->state.country!=u.state.country){send_raid_request_error(u,4,id,w);return true;}
        auto* target_party=party(*target);
        if(target_party || target->state.inviter_id!=0){send_raid_request_error(u,party(u) && party(u)==target_party?5U:3U,id,w);return true;}
        if(target->state.activity!=0){send_raid_request_error(u,1,id,w);return true;}
        target->state.inviter_id=u.member.id;const auto now=w.tick();target->state.invitation_deadline=now+30000U;
        net::Packet reply;append(reply,op,2);append(reply,u.member.id);w.send_user(target->member,reply);return true;
    }
    packet.at(22)=0;PartyRaidRequestLock lock(u,w);std::string name;
    for(std::size_t i=2;packet.at(i)!=0;++i)name.push_back(static_cast<char>(packet.at(i)));
    auto target=world.find_user_name(name);
    if(!target){send_party_request_error(u,4,0,w);return true;}retained(*target);
    if(!may_invite(u,w)){send_party_request_error(u,2,target->member.id,w);return true;}
    auto* cached_party=party(u);
    if(u.state.activity!=0){send_party_request_error(u,1,target->member.id,w);return true;}
    if(target->member.id==u.member.id || target->state.scene_state!=4 || target->state.zone!=u.state.zone || target->member.instance!=u.member.instance || target->state.country!=u.state.country){send_party_request_error(u,4,target->member.id,w);return true;}
    if(target->member.party || target->state.inviter_id!=0){send_party_request_error(u,cached_party && cached_party==target->member.party?5:3,target->member.id,w);return true;}
    if(target->state.activity!=0){send_party_request_error(u,1,target->member.id,w);return true;}
    target->state.inviter_id=u.member.id;const auto now=w.tick();target->state.invitation_deadline=now+30000U;
    if(u.member.name.size()>20 || u.member.name.find('\0')!=std::string::npos)throw std::invalid_argument("Invalid raid invitation name");
    net::Packet reply{0x18,0xb};reply.resize(23,0);for(std::size_t i=0;i<u.member.name.size();++i)reply[2+i]=static_cast<std::uint8_t>(u.member.name[i]);w.send_user(target->member,reply);return true;
}
}
