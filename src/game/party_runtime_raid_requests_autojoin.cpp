#include "eternity/game/party_runtime_raid_requests_autojoin.hpp"
#include "eternity/game/party_runtime_raid_requests_lock.hpp"
#include <stdexcept>
namespace eternity::game {
void send_raid_request_error(const PartyRequestUser& u,std::uint32_t reason,std::uint32_t id,PartyMembershipWorld& w){
    net::Packet b{0x19,0xb};for(auto n:{reason,id})for(unsigned i=0;i<4;++i)b.push_back(static_cast<std::uint8_t>(n>>(8*i)));w.send_user(u.member,b);
}
bool dispatch_raid_autojoin_request(const PartyRequestUser& u,net::Packet& request,PartyRaidRequestsWorld& world){
    if(request.at(0)!=0x11 || request.at(1)!=0xb)return false;
    if(u.member.party)return true; // Before terminator mutation or lookup.
    request.at(22)=0;std::string name;
    for(std::size_t i=2;request.at(i)!=0;++i)name.push_back(static_cast<char>(request.at(i)));
    auto found=world.find_active_raid_user_name(name);auto& w=world.membership_world();
    if(!found){send_raid_request_error(u,12,0,w);return true;}
    if(!found->lifetime)throw std::logic_error("Active raid lookup returned an unretained user");
    PartyRaidRequestLock lock(*found,w);auto* p=static_cast<PartyRegistryParty*>(found->member.party);
    if(!p){send_raid_request_error(u,12,0,w);return true;}
    // GetPartyBoth 00444f20 captures pointer under lock, before unlock callbacks.
    w.lock(*p);auto* leader=p->count>0?raid_membership_slot(*p,p->leader):nullptr;w.unlock(*p);
    if(!leader || u.state.country!=found->state.country){send_raid_request_error(u,12,0,w);return true;}
    p=static_cast<PartyRegistryParty*>(found->member.party);
    if(!p)throw std::logic_error("Leader lookup callback invalidated required live raid party");
    if(!p->auto_join){send_raid_request_error(u,13,0,w);return true;}
    if(p->count>=30){send_raid_request_error(u,14,0,w);return true;}
    // AutoPartyJoin 004473d0 tests NONZERO kind, not kind==1; failure is silent.
    if(p->experience.kind!=0 && p->auto_join)enter_raid_party(*p,u.member,w);
    return true;
}
}
