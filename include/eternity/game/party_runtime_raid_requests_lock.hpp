#pragma once
#include "eternity/game/party_runtime_requests.hpp"
namespace eternity::game {
// CPartyLock 00444d20/00444d60: retain the ORIGINAL captured lock even if a
// callback switches the user's current party; drop it only if membership clears.
class PartyRaidRequestLock {
    PartyNormalMembership* captured_;PartyMembershipWorld& world_;
public:
    PartyRaidRequestLock(const PartyRequestUser& u,PartyMembershipWorld& w):captured_(u.member.party),world_(w){if(captured_){world_.lock(*captured_);if(!u.member.party){world_.unlock(*captured_);captured_=nullptr;}}}
    ~PartyRaidRequestLock(){if(captured_)world_.unlock(*captured_);}
    PartyRaidRequestLock(const PartyRaidRequestLock&)=delete;
    PartyRaidRequestLock& operator=(const PartyRaidRequestLock&)=delete;
};
}
