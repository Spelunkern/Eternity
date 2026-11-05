#pragma once
#include "eternity/game/party_runtime_membership_send.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <list>
#include <limits>
#include <new>
#include <optional>
#include <vector>
#include <stdexcept>

namespace eternity::game {
// Serialized owner of one live SConnection's prioritized queue. The pool must
// outlive the queue and each buffer transferred by take_first(). Each queued
// entry owns one buffer reference. The caller routes drained buffers to the
// actual connection worker and releases the transferred reference on completion.
class UserPriorityQueue final {
    struct Entry {std::int32_t key;PartyMembershipSendBuffer* buffer;};
public:
    explicit UserPriorityQueue(PartyMembershipSendWorld& pool):pool_(pool){}
    ~UserPriorityQueue(){for(const auto& entry:entries_)release_party_membership_buffer(*entry.buffer,pool_);}
    UserPriorityQueue(const UserPriorityQueue&)=delete;UserPriorityQueue& operator=(const UserPriorityQueue&)=delete;
    // 004bd6f0/004e02a0: distance is truncated with x87 integer-indefinite
    // on NaN/overflow; priority and distance are ORed, then signed-sorted.
    bool enqueue(std::int32_t socket,PartyMembershipSendBuffer& buffer,std::uint32_t priority,float distance) {
        if(socket==-1 || std::bit_cast<std::int32_t>(bytes_)>=2048)return false;
        if(buffer.frame.size()<2)throw std::invalid_argument("Missing priority packet frame");
        const auto length=static_cast<std::uint16_t>(buffer.frame[0]|(static_cast<unsigned>(buffer.frame[1])<<8));
        if(length!=buffer.frame.size())throw std::invalid_argument("Invalid priority packet frame");
        const auto integral=std::trunc(static_cast<double>(distance));
        const auto distance_bits=(!std::isfinite(integral) || integral<INT32_MIN || integral>INT32_MAX)
            ?0x80000000U:static_cast<std::uint32_t>(static_cast<std::int32_t>(integral));
        const auto key=std::bit_cast<std::int32_t>((priority<<16)|distance_bits);
        buffer.references.fetch_add(1,std::memory_order_seq_cst);
        try {
            auto at=entries_.begin();while(at!=entries_.end() && at->key<=key)++at;
            entries_.insert(at,{key,&buffer});
        } catch(const std::bad_alloc&) {release_party_membership_buffer(buffer,pool_);return false;}
        bytes_+=length;return true;
    }
    // 004e04c0 allocates a batch before mutating the queue, then transfers at
    // most 128 references in priority order. Nullopt means empty or allocation
    // failure; the caller owns every returned reference until send completion.
    std::optional<std::vector<PartyMembershipSendBuffer*>> take_batch() {
        if(entries_.empty())return {};
        std::vector<PartyMembershipSendBuffer*> batch;
        try {batch.reserve(std::min<std::size_t>(128,entries_.size()));}catch(const std::bad_alloc&){return {};}
        while(batch.size()<128 && !entries_.empty())batch.push_back(take_first());
        return batch;
    }
    // Transfers the oldest lowest-key reference; the connection worker owns it.
    PartyMembershipSendBuffer* take_first() {
        if(entries_.empty())return nullptr;
        auto* buffer=entries_.front().buffer;bytes_-=static_cast<std::uint16_t>(buffer->frame[0]|(static_cast<unsigned>(buffer->frame[1])<<8));
        entries_.pop_front();return buffer;
    }
    // SSyncQueueBufferPriority::Reset 004dfe60 releases all remaining refs.
    void reset() {while(!entries_.empty()){auto* buffer=take_first();release_party_membership_buffer(*buffer,pool_);}}
    bool owns(const PartyMembershipSendWorld& pool) const noexcept{return &pool_==&pool;}
    std::uint32_t bytes() const noexcept{return bytes_;}
    std::size_t size() const noexcept{return entries_.size();}
private:PartyMembershipSendWorld& pool_;std::list<Entry> entries_;std::uint32_t bytes_=0;
};
}
