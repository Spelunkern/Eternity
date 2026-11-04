#include "eternity/game/party_runtime_requests.hpp"
#include <stdexcept>
namespace eternity::game {
namespace {
void append(net::Packet& p,std::uint32_t n,unsigned bytes=4) {for(unsigned i=0;i<bytes;++i)p.push_back(static_cast<std::uint8_t>(n>>(8*i)));}
std::uint32_t word(const net::Packet& p,std::size_t at,unsigned size) {std::uint32_t n=0;for(unsigned i=0;i<size;++i)n|=static_cast<std::uint32_t>(p.at(at+i))<<(8*i);return n;}
PartyRegistryParty* party(const PartyRequestUser& user) {return static_cast<PartyRegistryParty*>(user.member.party);}
PartyMembershipUser*& slot(PartyRegistryParty& p,std::size_t i) {return i<7?p.slots[i]:p.extra_slots[i-7];}
class MembershipLock {
    PartyMembershipWorld& world_;PartyNormalMembership* retained_;
public:
    MembershipLock(PartyRequestUser& u,PartyMembershipWorld& world):world_(world),retained_(u.member.party) {
        if(retained_) {world_.lock(*retained_);if(!u.member.party){world_.unlock(*retained_);retained_=nullptr;}}
    }
    ~MembershipLock() {if(retained_)world_.unlock(*retained_);}
};
bool leader(PartyRegistryParty& p,PartyMembershipUser& u,PartyMembershipWorld& world) {
    world.lock(p);const bool result=p.count>0 && slot(p,static_cast<std::size_t>(p.leader))==&u;world.unlock(p);return result;
}
void error(const PartyRequestUser& u,std::uint8_t code,std::uint32_t id,PartyMembershipWorld& w) {
    net::Packet p;append(p,0xb03,2);append(p,code,1);append(p,id);w.send_user(u.member,p);
}
bool enter(PartyRegistryParty& p,PartyRequestUser& u,PartyRequestWorld& w) {
    return p.experience.kind==1?w.enter_raid(p,u):enter_normal_party(p,u.member,w.membership_world());
}
void leave(PartyRegistryParty& p,PartyRequestUser& u,PartyRequestWorld& w) {
    if(p.experience.kind==1)w.leave_raid(p,u);else leave_normal_party(p,u.member,w.membership_world());
}
PartyMembershipUser* find_member(PartyRegistryParty& p,std::uint32_t id,PartyMembershipWorld& w) {
    w.lock(p);std::int32_t selected=-1;
    if(p.experience.kind<=1) {const auto count=p.count;for(std::int32_t i=0;i<count;++i){auto* u=slot(p,static_cast<std::size_t>(i));if(u && u->id==id){selected=i;break;}}}
    w.unlock(p);return selected<0?nullptr:slot(p,static_cast<std::size_t>(selected));
}
bool set_leader(PartyRegistryParty& p,std::uint32_t id,PartyMembershipWorld& w) {
    w.lock(p);
    for(std::int32_t i=0;i<p.count;++i) {
        if(slot(p,static_cast<std::size_t>(i))->id!=id || i==p.leader)continue;
        const auto old=p.leader;p.leader=i;net::Packet packet;append(packet,0xb07,2);append(packet,id);w.send_party(p,packet);
        auto* next=slot(p,static_cast<std::size_t>(i));auto* previous=slot(p,static_cast<std::size_t>(old));w.unlock(p);
        if(next)notify_party_membership_scene(*next,w);
        if(previous)notify_party_membership_scene(*previous,w);
        return true;
    }
    w.unlock(p);return false;
}
void pinned(const PartyRequestUser& u) {if(!u.lifetime)throw std::invalid_argument("Unretained party request user");}
bool raid_opcode(std::uint32_t op) {return op>=0xb0e && op<=0xb1a && op!=0xb19;}
}
void dispatch_party_request(PartyRequestUser self,net::Packet& packet,PartyRequestWorld& world) {
    pinned(self);auto& w=world.membership_world();const auto op=word(packet,0,2);
    if(raid_opcode(op)) {world.raid_request(self,packet);return;}
    if(op==0xb0c)return;
    if(op==0xb02 || op==0xb09) {
        std::uint32_t target_id=op==0xb02?word(packet,2,4):0;
        if(op==0xb02) {
            if(target_id==self.member.id){error(self,4,target_id,w);return;}
            if(self.state.activity!=0){error(self,1,target_id,w);return;}
        } else {packet.at(22)=0;}
        MembershipLock held(self,w);
        std::optional<PartyRequestUser> target;
        PartyRegistryParty* name_party=nullptr;
        if(op==0xb09) {
            std::size_t size=0;while(packet.at(2+size)!=0)++size;
            auto found=world.find_user_name(std::string_view(reinterpret_cast<const char*>(packet.data()+2),size));
            if(found)target.emplace(*found);
            if(!target){error(self,4,0,w);return;}pinned(*target);
        }
        auto* current=party(self);
        if(current) {
            if(!leader(*current,self.member,w) || party(self)->count>=7){error(self,2,op==0xb02?target_id:target->member.id,w);return;}
            name_party=party(self);
        }
        if(op==0xb02){auto found=world.find_zone_user(self,target_id);if(found)target.emplace(*found);}
        if(!target){error(self,4,target_id,w);return;}pinned(*target);
        if(op==0xb09) {
            target_id=target->member.id;
            if(self.state.activity!=0){error(self,1,target_id,w);return;}
            if(target_id==self.member.id || target->state.scene_state!=4 || target->state.zone!=self.state.zone || target->member.instance!=self.member.instance || target->state.country!=self.state.country){error(self,4,target_id,w);return;}
        } else if(target->state.country!=self.state.country){error(self,4,target_id,w);return;}
        if(target->member.party || target->state.inviter_id!=0) {
            auto* source=op==0xb09?name_party:party(self);
            error(self,source && source==party(*target)?5:3,target_id,w);return;
        }
        if(target->state.activity!=0){error(self,1,target_id,w);return;}
        target->state.inviter_id=self.member.id;
        target->state.invitation_deadline=w.tick()+30000U;
        net::Packet invite;append(invite,op,2);
        if(op==0xb02)append(invite,self.member.id);
        else {const auto at=invite.size();invite.resize(at+21,0);if(self.member.name.size()>20)throw std::invalid_argument("Invalid inviter name");for(std::size_t i=0;i<self.member.name.size();++i)invite[at+i]=static_cast<std::uint8_t>(self.member.name[i]);}
        w.send_user(target->member,invite);return;
    }
    if(op==0xb03) {
        if(self.state.inviter_id==0 || self.member.party)return;
        auto inviter=world.find_zone_user(self,self.state.inviter_id);
        self.state.inviter_id=0;self.state.invitation_deadline=0;
        if(!inviter){return;}pinned(*inviter);
        if(inviter->state.activity!=0 || self.state.activity!=0){error(*inviter,1,self.member.id,w);return;}
        if(packet.at(2)!=0){error(*inviter,0,self.member.id,w);return;}
        bool joined=false;
        {
            MembershipLock held(*inviter,w);auto* p=party(*inviter);
            if(!p)p=create_registry_party(world.registry(),&inviter->member,1,world.registry_ports(),w);
            else if(!leader(*p,inviter->member,w))p=nullptr;
            if(p)joined=enter(*p,self,world);
        }
        if(!joined)error(*inviter,1,self.member.id,w);
        return;
    }
    if(op==0xb05) {if(auto* p=party(self))leave(*p,self,world);return;}
    if(op==0xb0d) {MembershipLock held(self,w);if(auto* p=party(self))leave(*p,self,world);return;}
    if(op==0xb06) {
        auto* p=party(self);if(!p || !leader(*p,self.member,w))return;
        if(auto* member=find_member(*p,word(packet,2,4),w)){auto target=world.retain_member(*member);pinned(target);if(&target.member!=member)throw std::logic_error("Party retainer changed member identity");world.expel_user(*p,target);}return;
    }
    if(op==0xb07) {
        MembershipLock held(self,w);auto* p=party(self);
        if(!p || !leader(*p,self.member,w) || !set_leader(*party(self),word(packet,2,4),w)){net::Packet result;append(result,0xb07,2);append(result,0);w.send_user(self.member,result);}return;
    }
    world.close_connection(self,9,0);
}
}
