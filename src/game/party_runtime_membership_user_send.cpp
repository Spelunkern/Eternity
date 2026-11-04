#include "eternity/game/party_runtime_membership_user_send.hpp"
namespace eternity::game {
void send_party_membership_user(PartyMembershipUser& user,std::span<const std::uint8_t> payload,PartyMembershipUserSendWorld& world) {
    if(payload.size()>2048) {
        if(user.send_error==0)user.send_error=12;
        // Original stores zero only if +0x1c already equals zero; existing
        // nonzero details are retained. Neither case changes its value.
        const auto socket=user.send_socket;
        if(socket!=-1) {
            world.close_send_socket(user,socket);
            user.send_socket=-1;
        }
        return;
    }
    if(user.send_socket==-1)return;
    auto* buffer=allocate_party_membership_send_buffer(payload,world);
    if(!buffer)return;
    // The connection is intentionally not rechecked after allocation.
    world.enqueue_send(user,*buffer);
}
void PartyMembershipUserSendWorld::send_user(PartyMembershipUser& user,const net::Packet& packet) {
    send_party_membership_user(user,packet,*this);
}
}
