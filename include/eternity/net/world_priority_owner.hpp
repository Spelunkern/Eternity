#pragma once
#include "eternity/net/world_priority_flush.hpp"
#include <asio/post.hpp>
#include <exception>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

namespace eternity::net {
// One world-strand owner for a single real channel and its actual priority
// queue. Retain it, its socket and both executors until its drain callback has
// returned. That drain does not replace transport, session, SQL or service
// drains. Pool and callbacks must not throw. A failed cross-strand notification
// or poll post is observable and retryable; no callback is fabricated.
class WorldPriorityOwner final:public std::enable_shared_from_this<WorldPriorityOwner> {
public:
    using Strand=WorldPriorityBatch::Strand;
    using Pool=game::PartyMembershipSendWorld;
    static std::shared_ptr<WorldPriorityOwner> create(Strand strand,std::shared_ptr<Pool> pool,
        WorldChannel channel,std::function<void(bool)> result,std::function<void()> before_poll_post={}) {
        if(!strand.running_in_this_thread() || !pool || !result || !channel.connection_identity ||
           !channel.pending_send_state || !channel.send_batch_tracked)
            throw std::invalid_argument("Priority owner requires its strand and actual owners");
        return std::shared_ptr<WorldPriorityOwner>(new WorldPriorityOwner(std::move(strand),std::move(pool),std::move(channel),std::move(result),std::move(before_poll_post)));
    }
    game::UserPriorityQueue& queue() {require_strand();if(closed_)throw std::logic_error("Priority owner is closed");return queue_;}
    void flush() {
        require_strand();if(closed_ || polling_ || callback_active_ || delivering_)throw std::logic_error("Priority owner cannot flush now");
        auto self=shared_from_this();
        try {flush_world_priority(strand_,queue_,pool_,channel_,handles_,active_,[self](bool ok){self->on_result(ok);});}
        catch(...) {if(closed_)poll();throw;}
        if(closed_)poll();
    }
    // One registration. Close/reset can be requested reentrantly during a
    // queue release or result callback; completion waits for both to return.
    void close_and_wait(std::function<void()> done) {
        require_strand();if(closed_ || !done)throw std::invalid_argument("Priority owner requires one drain callback");
        closed_=true;done_=std::move(done);keep_=shared_from_this();
        if(!active_ && !polling_ && !callback_active_)poll();
    }
    // Called on the world strand after failed posts or to poll outstanding
    // real completions; no absent transport result is converted to success.
    void retry() {require_strand();if(active_ || polling_ || callback_active_ || delivering_)return;poll();}
    bool closed() const {require_strand();return closed_;}
    bool drained() const {require_strand();return drained_;}
    std::size_t outstanding() const {require_strand();return handles_.size();}
    std::exception_ptr poll_post_error() const {require_strand();return poll_error_;}
    std::size_t failed_notifications() const {
        require_strand();std::size_t count=0;for(const auto& handle:handles_)if(handle->post_error())++count;return count;
    }
private:
    WorldPriorityOwner(Strand strand,std::shared_ptr<Pool> pool,WorldChannel channel,
        std::function<void(bool)> result,std::function<void()> before_poll_post)
        :strand_(std::move(strand)),pool_(std::move(pool)),queue_(*pool_),channel_(std::move(channel)),result_(std::move(result)),before_poll_post_(std::move(before_poll_post)) {}
    void require_strand() const {if(!strand_.running_in_this_thread())throw std::logic_error("Priority owner requires its world strand");}
    void on_result(bool ok) {
        require_strand();auto self=shared_from_this();callback_active_=true;
        result_(ok);callback_active_=false;
        // The batch's completion flag is set only after this callback returns.
        if(closed_)schedule_poll();
    }
    void schedule_poll() {
        if(poll_posted_)return;
        try {
            if(before_poll_post_)before_poll_post_();
            asio::post(strand_,[self=shared_from_this()]{self->poll_posted_=false;self->poll();});
            poll_posted_=true;poll_error_={};
        } catch(...) {poll_error_=std::current_exception();}
    }
    void poll() {
        require_strand();if(active_ || polling_ || callback_active_ || delivering_)return;
        auto self=shared_from_this();polling_=true;
        try {
            for(const auto& handle:handles_)if(!handle->complete())handle->retry();
            std::erase_if(handles_,[](const auto& handle){return handle->complete();});
            if(closed_)queue_.reset();
        } catch(...) {polling_=false;throw;}
        polling_=false;
        if(closed_ && handles_.empty() && !drained_) {
            delivering_=true;poll_error_={};auto done=std::move(done_);done();
            delivering_=false;drained_=true;keep_.reset();
        }
    }
    Strand strand_;std::shared_ptr<Pool> pool_;game::UserPriorityQueue queue_;WorldChannel channel_;
    std::function<void(bool)> result_;std::function<void()> before_poll_post_,done_;
    std::vector<std::shared_ptr<WorldPriorityBatch>> handles_;std::shared_ptr<WorldPriorityOwner> keep_;
    std::exception_ptr poll_error_;bool active_=false,polling_=false,callback_active_=false,delivering_=false,closed_=false,drained_=false,poll_posted_=false;
};
}
