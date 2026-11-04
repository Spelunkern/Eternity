#include "eternity/game/party_runtime_raid_requests_expel.hpp"
#include "eternity/game/party_runtime_raid_requests_lock.hpp"
#include <stdexcept>
namespace eternity::game {
namespace {
PartyRegistryParty* current(const PartyRequestUser& u){return static_cast<PartyRegistryParty*>(u.member.party);}
}
bool dispatch_raid_expulsion_request(const PartyRequestUser& u,net::Packet& request,PartyRaidRequestsWorld& world){
    if(request.at(0)!=0x1a || request.at(1)!=0xb)return false;
    auto& w=world.membership_world();PartyRaidRequestLock lock(u,w);auto* p=current(u);
    if(!p || p->experience.kind==0 || !is_party_request_leader(*p,u.member,w))return true;
    std::uint32_t id=0;for(unsigned i=0;i<4;++i)id|=static_cast<std::uint32_t>(request.at(2+i))<<(8*i);
    p=current(u);if(!p)throw std::logic_error("Leader callback invalidated required raid party");
    auto* selected=find_party_request_member(*p,id,w);if(!selected)return true;
    auto target=world.retain_member(*selected);
    if(!target.lifetime || &target.member!=selected)throw std::logic_error("Raid retainer changed selected user identity");
    p=current(u);if(!p)throw std::logic_error("Find callback invalidated required raid party");
    expel_raid_party(*p,target.member,w);return true;
}
}
