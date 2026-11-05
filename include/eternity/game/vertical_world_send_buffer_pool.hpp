#pragma once
#include "eternity/game/party_runtime_membership_send.hpp"
#include <memory>
#include <mutex>
#include <stdexcept>
#include <unordered_map>

namespace eternity::game {
// Native backing storage for recovered SPacketBufferSend references. Allocated
// buffers remain owned until the final reference calls free_send_buffer.
// It supplies the recovered size classes, not the original heap's capacity or
// recycling policy. The owner outlives every queue and outstanding reference.
// ps_game.exe SPacketBufferSend::AllocBuffer 004df9f0 / release call 00445659.
class VerticalWorldSendBufferPool final {
public:
    VerticalWorldSendBufferPool()=default;
    VerticalWorldSendBufferPool(const VerticalWorldSendBufferPool&)=delete;
    VerticalWorldSendBufferPool& operator=(const VerticalWorldSendBufferPool&)=delete;
    ~VerticalWorldSendBufferPool() {if(!buffers_.empty())std::terminate();}
    PartyMembershipSendBuffer* allocate_send_buffer(std::uint16_t capacity) {
        if(capacity!=32 && capacity!=128 && capacity!=2048)throw std::invalid_argument("Invalid send buffer size class");
        try {
            auto buffer=std::make_unique<PartyMembershipSendBuffer>();buffer->frame.reserve(capacity);buffer->capacity=capacity;
            auto* result=buffer.get();std::lock_guard lock(mutex_);buffers_.emplace(result,std::move(buffer));return result;
        } catch(const std::bad_alloc&) {return nullptr;}
    }
    void free_send_buffer(PartyMembershipSendBuffer& buffer) noexcept {
        std::lock_guard lock(mutex_);auto found=buffers_.find(&buffer);
        if(found==buffers_.end() || buffer.references.load(std::memory_order_seq_cst)!=0)std::terminate();
        buffers_.erase(found);
    }
    std::size_t outstanding() const {std::lock_guard lock(mutex_);return buffers_.size();}
private:
    mutable std::mutex mutex_;std::unordered_map<PartyMembershipSendBuffer*,std::unique_ptr<PartyMembershipSendBuffer>> buffers_;
};
}
