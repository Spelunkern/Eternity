#pragma once
#include "eternity/game/party_runtime_membership_send.hpp"
namespace eternity::game {
class PartyMembershipUserSendWorld:public PartyMembershipSendWorld {
public:
    // Required actual connection close; original closes the captured socket,
    // then writes disconnected state after this callback returns.
    virtual void close_send_socket(PartyMembershipUser&,std::int32_t socket)=0;
    void send_user(PartyMembershipUser&,const net::Packet&) override;
};
// ps_game.exe SConnection::Send(void*,int) 004d4d20. Direct send transfers its
// initial reference to the worker; there is no extra retain/sender release.
// Payload <=2046 is safe; >2048 follows the original error/close branch.
// 2047..2048 would overrun the original 2048-byte pool and remains unsupported
// by the safe recovered allocator. This range is never a normal-party roster.
void send_party_membership_user(PartyMembershipUser&,std::span<const std::uint8_t>,PartyMembershipUserSendWorld&);
}
