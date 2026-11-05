#pragma once
#include "eternity/game/party_runtime_membership_send.hpp"
#include "eternity/game/user_view_publication.hpp"
#include "eternity/game/user_priority_queue.hpp"
#include <algorithm>

namespace eternity::game {
// Reuses the existing SPacketBufferSend pool and SConnection send provider.
// Pool and queued references must outlive this adapter and any returned buffer;
// free/enqueue must not throw or synchronously invalidate the target. Each
// successful enqueue owns one reference until its completion callback releases it.
class UserViewSharedBuffers final {
    class OwnedBuffer final:public UserViewBuffer {
    public:
        OwnedBuffer(PartyMembershipSendWorld& world,PartyMembershipSendBuffer& value):world_(world),value_(&value){}
        ~OwnedBuffer() override {release_party_membership_buffer(*value_,world_);}
        PartyMembershipSendWorld& world_;PartyMembershipSendBuffer* value_;
    };
public:
    explicit UserViewSharedBuffers(PartyMembershipSendWorld& world):world_(world){}
    std::unique_ptr<UserViewBuffer> allocate(std::span<const std::uint8_t> payload) {
        if(payload.size()>2046)throw std::length_error("User view packet exceeds original buffer capacity");
        const std::uint16_t capacity=payload.size()<31?32:payload.size()<127?128:2048;
        auto* value=world_.allocate_send_buffer(capacity);if(!value)return {};
        value->references.store(1,std::memory_order_seq_cst);value->custom_references=0;value->capacity=capacity;
        try {
            value->frame.resize(payload.size()+2);const auto length=static_cast<std::uint16_t>(value->frame.size());
            value->frame[0]=static_cast<std::uint8_t>(length);value->frame[1]=static_cast<std::uint8_t>(length>>8);
            std::copy(payload.begin(),payload.end(),value->frame.begin()+2);
            return std::make_unique<OwnedBuffer>(world_,*value);
        } catch(...) {release_party_membership_buffer(*value,world_);throw;}
    }
    void enqueue(PartyMembershipUser& target,UserViewBuffer& abstract) {
        auto* owned=dynamic_cast<OwnedBuffer*>(&abstract);
        if(!owned || &owned->world_!=&world_)throw std::invalid_argument("Foreign user view buffer");
        if(target.send_socket==-1)return;
        auto& value=*owned->value_;value.references.fetch_add(1,std::memory_order_seq_cst);
        try {world_.enqueue_send(target,value);}catch(...) {release_party_membership_buffer(value,world_);throw;}
    }
    bool enqueue_priority(PartyMembershipUser& target,UserViewBuffer& abstract,UserPriorityQueue& queue,std::uint32_t priority,float distance) {
        auto* owned=dynamic_cast<OwnedBuffer*>(&abstract);
        if(!owned || &owned->world_!=&world_ || !queue.owns(world_))throw std::invalid_argument("Foreign user view buffer or queue");
        return queue.enqueue(target.send_socket,*owned->value_,priority,distance);
    }
private:
    PartyMembershipSendWorld& world_;
};
}
