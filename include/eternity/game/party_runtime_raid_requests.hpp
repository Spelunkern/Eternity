#pragma once
#include "eternity/game/party_runtime_raid.hpp"
namespace eternity::game {
// ps_game.exe SetRaidParty 004458f0. Mutates kind and active wire indices BEFORE
// its internal recursive lock; retains pointers/count/leader/cursor/experience.
void configure_raid_party(PartyRegistryParty&,std::uint8_t,PartyMembershipWorld&);
// Concrete PacketParty conversion/configuration/disband/leadership branches. The inherited
// entry/leave and default ExpelUser use the SAME live retained membership owner.
class PartyRaidRequestsWorld:public PartyRaidRequestWorld {
public:
    // CWorld::FindUser(char*) 0040d690 active-name index. This is NOT the
    // existing FindUserZoneMove provider; retain the SAME live member record.
    virtual std::optional<PartyRequestUser> find_active_raid_user_name(std::string_view)=0;
    void raid_request(const PartyRequestUser&,net::Packet&) override;
    // Defensive direct-call fallback; ordinary PacketParty raid opcodes are
    // recovered. Default uses the required actual close provider (9,0).
    virtual void remaining_raid_request(const PartyRequestUser&,net::Packet&);
};
}
