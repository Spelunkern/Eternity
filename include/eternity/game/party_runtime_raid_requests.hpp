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
    void raid_request(const PartyRequestUser&,net::Packet&) override;
    // Required remaining original branches (invites, autojoin), never a
    // default success/no-op.
    // Also required for autojoin bytes >1: current registry bool cannot retain
    // those original raw +115 values. Ordinary decoded booleans use real code.
    virtual void remaining_raid_request(const PartyRequestUser&,net::Packet&)=0;
};
}
