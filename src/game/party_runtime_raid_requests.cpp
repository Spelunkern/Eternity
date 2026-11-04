#include "eternity/game/party_runtime_raid_requests.hpp"
#include "eternity/game/party_runtime_registry_break.hpp"
#include <bit>
#include <stdexcept>
namespace eternity::game {
namespace {
std::uint32_t word(const net::Packet& p,std::size_t at,unsigned bytes=4) {
    std::uint32_t n=0;for(unsigned i=0;i<bytes;++i)n|=static_cast<std::uint32_t>(p.at(at+i))<<(8*i);return n;
}
void append(net::Packet& p,std::uint32_t n,unsigned bytes=4){for(unsigned i=0;i<bytes;++i)p.push_back(static_cast<std::uint8_t>(n>>(8*i)));}
PartyRegistryParty* party(const PartyRequestUser& u){return static_cast<PartyRegistryParty*>(u.member.party);}
struct Lock {
    PartyNormalMembership* captured;PartyMembershipWorld& world;
    Lock(const PartyRequestUser& u,PartyMembershipWorld& w):captured(u.member.party),world(w){if(captured){world.lock(*captured);if(!u.member.party){world.unlock(*captured);captured=nullptr;}}}
    ~Lock(){if(captured)world.unlock(*captured);}
};
bool is_leader(PartyRegistryParty& p,const PartyRequestUser& u){return raid_membership_slot(p,p.leader)==&u.member;}
}
void configure_raid_party(PartyRegistryParty& p,std::uint8_t kind,PartyMembershipWorld& w) {
    p.experience.kind=kind;
    for(std::int32_t i=0;i<p.count;++i)raid_membership_index(p,i)=static_cast<std::uint32_t>(i);
    w.lock(p);
    for(std::int32_t i=0;i<p.count;++i)if(i!=p.leader && i!=p.sub_leader){p.sub_leader=i;break;}
    w.unlock(p);
}
void PartyRaidRequestsWorld::raid_request(const PartyRequestUser& u,net::Packet& request) {
    const auto op=word(request,0,2);
    if(op!=0xb0e && op!=0xb0f && op!=0xb10 && op!=0xb16){remaining_raid_request(u,request);return;}
    // Preserve unsupported raw bool values through a required provider rather
    // than coercing them and publishing a packet the original would not send.
    if((op==0xb0e && request.at(3)>1) || (op==0xb10 && request.at(2)>1)){remaining_raid_request(u,request);return;}
    auto& w=membership_world();Lock lock(u,w);auto* p=party(u);
    if(!p || !is_leader(*p,u))return; // Original direct slot guard, no count test.
    if(op==0xb0e) {
        if(request.at(2)==0)return;
        configure_raid_party(*p,1,w);
        // Original rereads the user's current party after the inner unlock.
        p=party(u);if(!p)throw std::logic_error("Conversion callback invalidated required live party");
        p->experience.kind=request.at(2);p->item_division=std::bit_cast<std::int32_t>(word(request,4));p->auto_join=request.at(3)!=0;
        net::Packet reply;append(reply,op,2);append(reply,p->experience.kind,1);append(reply,p->auto_join?1U:0U,1);append(reply,static_cast<std::uint32_t>(p->item_division));append(reply,raid_membership_index(*p,p->leader));append(reply,raid_membership_index(*p,p->sub_leader));
        w.send_party(*p,reply);return;
    }
    if(p->experience.kind==0)return;
    net::Packet reply;append(reply,op,2);
    if(op==0xb0f){p->item_division=std::bit_cast<std::int32_t>(word(request,2));append(reply,word(request,2));w.send_party(*p,reply);}
    else if(op==0xb10){p->auto_join=request.at(2)!=0;append(reply,p->auto_join?1U:0U,1);w.send_party(*p,reply);}
    else {
        w.send_party(*p,reply);
        p=party(u);if(!p)throw std::logic_error("Disband callback invalidated required live party");
        break_registry_party(registry(),*p,registry_ports(),w);
    }
}
}
