#include "eternity/game/party_runtime_membership_send.hpp"
#include <stdexcept>

namespace eternity::game {
void release_party_membership_buffer(PartyMembershipSendBuffer& buffer,PartyMembershipSendWorld& world) {
    if(buffer.references.fetch_sub(1,std::memory_order_seq_cst)==1) world.free_send_buffer(buffer);
}
void send_party_membership(PartyNormalMembership& party,std::span<const std::uint8_t> payload,PartyMembershipSendWorld& world) {
    if(payload.size()>2046) throw std::length_error("Party send exceeds original buffer capacity");
    const std::uint16_t capacity=payload.size()<31?32:payload.size()<127?128:2048;
    auto* buffer=world.allocate_send_buffer(capacity);
    if(!buffer) return;
    buffer->capacity=capacity;
    buffer->references.store(1,std::memory_order_seq_cst);
    buffer->custom_references=0;
    buffer->frame.resize(payload.size()+2);
    const auto length=static_cast<std::uint16_t>(buffer->frame.size());
    buffer->frame[0]=static_cast<std::uint8_t>(length);
    buffer->frame[1]=static_cast<std::uint8_t>(length>>8);
    for(std::size_t i=0;i<payload.size();++i) buffer->frame[i+2]=payload[i];
    world.lock(party);
    // Kind is selected once, but count/slots are reread after each enqueue.
    if(party.experience.kind<=1) {
        for(std::int32_t i=0;i<party.count;++i) {
            auto* user=party.slots[static_cast<std::size_t>(i)];
            if(user && user->send_socket!=-1) {
                buffer->references.fetch_add(1,std::memory_order_seq_cst);
                world.enqueue_send(*user,*buffer);
            }
        }
    }
    world.unlock(party);
    release_party_membership_buffer(*buffer,world);
}
void PartyMembershipSendWorld::send_party(PartyNormalMembership& party,const net::Packet& packet) {
    send_party_membership(party,packet,*this);
}
}
