#pragma once
#include "eternity/game/party_runtime.hpp"
#include "eternity/net/framing.hpp"
#include <array>
#include <string>
#include <vector>

namespace eternity::game {
struct PartyNormalMembership;
struct PartyMembershipEffect {
    std::uint16_t skill{};
    std::uint8_t level{}, visibility{}; // definition +0x97: exactly 1 is omitted
    std::uint32_t deadline{};
};
struct PartyMembershipUser {
    std::uint32_t id{};
    std::uint32_t send_error{},send_error_detail{}; // SConnection +0x18/+0x1c, semantics beyond Send unresolved
    std::int32_t send_socket=-1; // SConnection +0x68; only -1 is disconnected
    std::string name; // original capacity 21, including terminator
    std::uint16_t level{}, hp{}, maximum_hp{}, mp{}, maximum_mp{}, sp{}, maximum_sp{}, instance{};
    std::uint8_t job{}, pending_request{};
    std::array<float,3> position{};
    bool in_zone{}, guild_creation{};
    PartyNormalMembership* party{};
    std::vector<PartyMembershipEffect> effects;
    std::size_t effect_cursor{};
};
struct PartyNormalMembership {
    std::uint32_t id{};
    std::int32_t leader{}, count{}, item_cursor{};
    // Only pUser shifts on leave; Index fields and inactive tail are retained.
    std::array<PartyMembershipUser*,7> slots{};
    std::array<std::uint32_t,7> slot_indices{};
    PartyRuntime experience{}; // normal BODY does not validate kind
};
class PartyMembershipWorld {
public:
    virtual ~PartyMembershipWorld()=default;
    virtual std::uint32_t tick()=0;
    // Original Windows critical sections are recursive; ownership must remain
    // valid across nested leave and synchronous callbacks. Callbacks must not throw.
    virtual void lock(PartyNormalMembership&)=0;
    virtual void unlock(PartyNormalMembership&) noexcept=0;
    virtual void lock_effects(PartyMembershipUser&)=0;
    virtual void unlock_effects(PartyMembershipUser&) noexcept=0;
    virtual void send_party(PartyNormalMembership&,const net::Packet&)=0;
    virtual void send_user(PartyMembershipUser&,const net::Packet&)=0;
    // Recovered CUser::GetPartyType 0048a330 (0 none/1 member/2 leader).
    // Default executes the original recursive lock/recheck algorithm.
    virtual std::uint8_t party_type(PartyMembershipUser&);
    virtual void send_scene(PartyMembershipUser&,const net::Packet&)=0;
    // Required real manager/guild owners, not default empty implementations.
    virtual void unregister_party(PartyNormalMembership&)=0;
    virtual void cleanup_guild_creation(PartyMembershipUser&)=0;
    virtual void fill_log(PartyMembershipUser&,std::span<std::uint8_t>)=0;
    virtual void send_log(const net::Packet&)=0;
};
// ps_game.exe CParty::EnterNormalParty 00445fe0 / LeaveNormalParty 00446960.
// Caller supplies a seven-slot projection with count 0..7 and non-null active
// slots. The original normal BODY has no kind guard; EnterUser selects it for
// every kind != 1. These do
// not implement invitations, raid conversion, manager allocation or transport.
// Shared recovered emitters, also used by registry force-disband.
void notify_party_membership_scene(PartyMembershipUser&,PartyMembershipWorld&);
void log_party_membership(PartyNormalMembership&,PartyMembershipUser&,std::uint16_t opcode,PartyMembershipWorld&);
bool enter_normal_party(PartyNormalMembership&,PartyMembershipUser&,PartyMembershipWorld&);
bool leave_normal_party(PartyNormalMembership&,PartyMembershipUser&,PartyMembershipWorld&);
}
