#pragma once
#include "eternity/game/party_runtime_registry.hpp"
#include <memory>
#include <optional>
#include <string_view>
namespace eternity::game {
struct PartyRequestState {
    std::uint32_t inviter_id{},invitation_deadline{}; // CUser +1740 / +5834
    std::int32_t activity{};                       // +15e0
    std::uint8_t country{};                        // +12d
    std::uint32_t scene_state{};                   // +5758
    std::uintptr_t zone{};                         // +e0 identity, not map number
};
// A retained live view, not another membership owner. All lookups/dispatches
// keep the Actor/connection context containing both referenced records alive.
struct PartyRequestUser {
    PartyMembershipUser& member;
    PartyRequestState& state;
    std::shared_ptr<void> lifetime;
};
class PartyRequestWorld {
public:
    virtual ~PartyRequestWorld()=default;
    virtual PartyMembershipWorld& membership_world()=0;
    virtual PartyRegistry& registry()=0;
    virtual PartyRegistryPorts& registry_ports()=0;
    virtual std::optional<PartyRequestUser> find_zone_user(const PartyRequestUser&,std::uint32_t)=0;
    virtual std::optional<PartyRequestUser> find_user_name(std::string_view)=0; // FindUserZoneMove(char*)
    // Lifetime acquisition only: return the same live record without dispatching
    // gameplay callbacks or modifying membership. This is not an original call.
    virtual PartyRequestUser retain_member(PartyMembershipUser&)=0;
    virtual void close_connection(const PartyRequestUser&,std::int32_t reason,std::int32_t detail)=0;
    // Required raid bodies; never default to success or normal Leave.
    virtual bool enter_raid(PartyRegistryParty&,const PartyRequestUser&)=0;
    virtual void leave_raid(PartyRegistryParty&,const PartyRequestUser&)=0;
    virtual void expel_user(PartyRegistryParty&,const PartyRequestUser&); // Real ExpelUser 00447120
    // Full original raid request branches are deliberately an explicit provider.
    virtual void raid_request(const PartyRequestUser&,net::Packet&)=0;
};
// ps_game.exe CUser::PacketParty 004676c0. Caller supplies a validated decoded
// packet (normal lengths 2/3/6/23 as appropriate) and a pinned live user. Name
// request 0b09 mutates byte 22 to NUL before lookup. Callbacks must not throw or
// invalidate selected party/slot lifetimes; real locks must be recursive.
// Shared recovered IsPartyBoth 00444e90 and FindUser 00445520 bodies. Find
// captures array position under lock, then reloads its pointer AFTER unlock.
void send_party_request_error(const PartyRequestUser&,std::uint8_t,std::uint32_t,PartyMembershipWorld&);
bool is_party_request_leader(PartyRegistryParty&,PartyMembershipUser&,PartyMembershipWorld&);
PartyMembershipUser* find_party_request_member(PartyRegistryParty&,std::uint32_t,PartyMembershipWorld&);
void dispatch_party_request(PartyRequestUser,net::Packet&,PartyRequestWorld&);
}
