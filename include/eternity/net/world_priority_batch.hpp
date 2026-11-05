#pragma once
#include "eternity/net/world_server.hpp"
#include "eternity/game/party_runtime_membership_send.hpp"
#include <asio/any_io_executor.hpp>
#include <asio/post.hpp>
#include <asio/strand.hpp>
#include <exception>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <vector>
#include <utility>

namespace eternity::net {
// Owns already-extracted priority references until the actual socket result
// reaches the pool's world strand. Keep this handle and both executors alive;
// retry() is required when posting the result back fails. The caller retains
// the original vector on rejection and owns its references in that case.
// Pool release and the completed callback must not throw. Transport stop alone
// does not wait for the later world-strand release or callback.
class WorldPriorityBatch final:public std::enable_shared_from_this<WorldPriorityBatch> {
public:
    using Strand=asio::strand<asio::any_io_executor>;
    using Buffer=game::PartyMembershipSendBuffer;
    using Pool=game::PartyMembershipSendWorld;
    static std::shared_ptr<WorldPriorityBatch> submit(Strand strand,std::shared_ptr<Pool> pool,
        std::vector<Buffer*>& batch,const WorldChannel& channel,std::function<void(bool)> completed,
        std::function<void()> before_post={}) {
        if(!strand.running_in_this_thread() || !pool || !channel.send_batch_tracked || !completed || batch.empty() || batch.size()>128)
            throw std::invalid_argument("Priority batch requires its world strand and real owners");
        std::vector<Packet> payloads;payloads.reserve(batch.size());
        for(const auto* buffer:batch) {
            if(!buffer || buffer->frame.size()<4 || buffer->frame.size()>2048 ||
               (std::size_t(buffer->frame[0])|(std::size_t(buffer->frame[1])<<8))!=buffer->frame.size())
                throw std::invalid_argument("Invalid priority batch frame");
            payloads.emplace_back(buffer->frame.begin()+2,buffer->frame.end());
        }
        auto self=std::shared_ptr<WorldPriorityBatch>(new WorldPriorityBatch(std::move(strand),std::move(pool),batch,std::move(completed),std::move(before_post)));
        self->keep_=self;
        bool accepted=false;
        try {accepted=channel.send_batch_tracked(payloads,[self](bool ok){self->socket_complete(ok);});}
        catch(...) {self->keep_.reset();throw;}
        if(!accepted){self->keep_.reset();return {};}
        batch.clear();return self;
    }
    // Must run on the same world strand. True means actual socket result and
    // reference release and the completion callback have both returned, not
    // remote receipt.
    bool complete() const {require_strand();return delivered_;}
    std::exception_ptr post_error() const {std::lock_guard lock(mutex_);return post_error_;}
    void retry() {require_strand();deliver();}
private:
    WorldPriorityBatch(Strand strand,std::shared_ptr<Pool> pool,const std::vector<Buffer*>& buffers,
        std::function<void(bool)> completed,std::function<void()> before_post)
        :strand_(std::move(strand)),pool_(std::move(pool)),buffers_(buffers),completed_(std::move(completed)),before_post_(std::move(before_post)) {}
    void require_strand() const {if(!strand_.running_in_this_thread())throw std::logic_error("Priority batch requires its world strand");}
    void socket_complete(bool ok) noexcept {
        {std::lock_guard lock(mutex_);result_=ok;}
        try {if(before_post_)before_post_();asio::post(strand_,[self=shared_from_this()]{self->deliver();});}
        catch(...) {std::lock_guard lock(mutex_);if(result_)post_error_=std::current_exception();}
    }
    void deliver() {
        require_strand();std::optional<bool> result;
        {std::lock_guard lock(mutex_);result=std::exchange(result_,{});post_error_={};}
        if(!result || delivered_)return;
        auto hold=shared_from_this();
        for(auto* buffer:buffers_)game::release_party_membership_buffer(*buffer,*pool_);
        buffers_.clear();keep_.reset();auto done=std::move(completed_);done(*result);
        delivered_=true;
    }
    Strand strand_;std::shared_ptr<Pool> pool_;std::vector<Buffer*> buffers_;std::function<void(bool)> completed_;
    std::function<void()> before_post_;mutable std::mutex mutex_;std::optional<bool> result_;std::exception_ptr post_error_;
    bool delivered_=false;std::shared_ptr<WorldPriorityBatch> keep_;
};
}
