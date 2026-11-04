#include "eternity/game/party_runtime_raid_requests_leaders.hpp"
#include "eternity/game/party_runtime_raid_requests_lock.hpp"
#include <stdexcept>
namespace eternity::game {
namespace {
std::uint32_t read(const net::Packet& p,std::size_t at,unsigned bytes=4){std::uint32_t n=0;for(unsigned i=0;i<bytes;++i)n|=static_cast<std::uint32_t>(p.at(at+i))<<(i*8);return n;}
void append(net::Packet& p,std::uint32_t n,unsigned bytes=4){for(unsigned i=0;i<bytes;++i)p.push_back(static_cast<std::uint8_t>(n>>(i*8)));}
void broadcast(PartyRegistryParty& p,std::uint16_t op,PartyMembershipUser* u,PartyMembershipWorld& w){if(!u)throw std::logic_error("Original raid leader broadcast requires a live selected user");net::Packet b;append(b,op,2);append(b,u->id);w.send_party(p,b);}
}
bool exchange_raid_wire_slots(PartyRegistryParty& p,std::uint32_t first,std::uint32_t second,PartyMembershipWorld& w){
    std::int32_t a=-1,b=-1;
    // Original searches BEFORE the internal lock and retains array positions.
    for(std::int32_t i=0;i<p.count;++i)if(raid_membership_index(p,i)==first){a=i;break;}
    for(std::int32_t i=0;i<p.count;++i)if(raid_membership_index(p,i)==second){b=i;break;}
    w.lock(p);
    if(a!=-1)raid_membership_index(p,a)=second;
    if(b!=-1)raid_membership_index(p,b)=first;
    w.unlock(p);return a!=-1 || b!=-1;
}
bool select_raid_sub_leader(PartyRegistryParty& p,std::uint32_t id,PartyMembershipWorld& w){
    w.lock(p);
    for(std::int32_t i=0;i<p.count;++i){auto* u=raid_membership_slot(p,i);if(u && i!=p.leader && u->id==id){p.sub_leader=i;break;}}
    broadcast(p,0xb13,raid_membership_slot(p,p.sub_leader),w);w.unlock(p);return true;
}
bool select_raid_leader(PartyRegistryParty& p,std::uint32_t id,PartyMembershipWorld& w){
    w.lock(p);std::int32_t selected=0;
    while(selected<p.count){auto* u=raid_membership_slot(p,selected);if(u && u->id==id && selected!=p.leader)break;++selected;}
    if(selected>=p.count){w.unlock(p);return false;}
    const auto previous=p.leader;p.leader=selected;
    broadcast(p,0xb17,raid_membership_slot(p,selected),w);
    if(selected==p.sub_leader){
        for(std::int32_t i=0;i<30;++i)if(i!=p.leader && raid_membership_slot(p,i)){p.sub_leader=i;break;}
        broadcast(p,0xb13,raid_membership_slot(p,p.sub_leader),w);
    }
    // Capture both pointers AFTER broadcasts, using selected/previous positions,
    // not a re-read of the potentially callback-mutated current leader index.
    auto* next=raid_membership_slot(p,selected);auto* old=raid_membership_slot(p,previous);
    w.unlock(p);
    if(next)notify_party_membership_scene(*next,w);
    if(old)notify_party_membership_scene(*old,w);
    return true;
}
bool dispatch_raid_leadership_request(const PartyRequestUser& u,net::Packet& request,PartyRaidRequestsWorld& world){
    auto op=read(request,0,2);if(op!=0xb12 && op!=0xb13 && op!=0xb17)return false;
    auto& w=world.membership_world();PartyRaidRequestLock lock(u,w);auto* p=static_cast<PartyRegistryParty*>(u.member.party);
    if(!p || p->experience.kind==0)return true;
    const bool leader=raid_membership_slot(*p,p->leader)==&u.member;
    if(!leader && (op!=0xb12 || raid_membership_slot(*p,p->sub_leader)!=&u.member))return true;
    if(op==0xb13){select_raid_sub_leader(*p,read(request,2),w);return true;}
    if(op==0xb17){select_raid_leader(*p,read(request,2),w);return true;}
    exchange_raid_wire_slots(*p,read(request,2),read(request,6),w);
    p=static_cast<PartyRegistryParty*>(u.member.party);if(!p)throw std::logic_error("Slot exchange callback invalidated required live party");
    net::Packet reply;append(reply,op,2);append(reply,read(request,2));append(reply,read(request,6));append(reply,raid_membership_index(*p,p->leader));append(reply,raid_membership_index(*p,p->sub_leader));w.send_party(*p,reply);return true;
}
}
