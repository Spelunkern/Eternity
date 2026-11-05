#pragma once
#include "eternity/game/db_dispatch.hpp"
#include "eternity/game/vertical_session_registry.hpp"
#include "eternity/net/framing.hpp"
#include <asio/any_io_executor.hpp>
#include <asio/post.hpp>
#include <asio/strand.hpp>
#include <exception>
#include <memory>
#include <vector>

namespace eternity::game {
// Registry and tracker methods run on the shared world strand. work executes on
// a DB worker and must serialize its SQL connections/cache and capture all
// associated outbound replies; deliver and failure run on the world strand.
// Tracker, registry, session and executor survive accepted jobs and callbacks.
// Stop producers and drain replies before releasing an account. Lost/stranded
// legacy queue work deliberately leaves the registry fenced. This does not
// fence unsolicited traffic outside this adapter or shutdown races in the queue.
template<class Session> class VerticalTrackedDbJobs final:public std::enable_shared_from_this<VerticalTrackedDbJobs<Session>> {
public:
    using Strand=asio::strand<asio::any_io_executor>;
    using Packets=std::vector<net::Packet>;
    // Work may request retries synchronously; they are registered on the
    // world strand before the current job fence is released.
    using Retry=std::function<void(std::uint32_t)>;
    using Work=std::function<Packets(Session&,std::uint32_t,const Retry&)>;
    using Deliver=std::function<void(Session&,Packets)>;
    using Failure=std::function<void(std::exception_ptr)>;
    static std::shared_ptr<VerticalTrackedDbJobs> create(Strand strand,VerticalSessionRegistry<Session>& registry,
        std::shared_ptr<Session> owner,std::uint32_t account,DbJobDispatcher& dispatcher,Work work,Deliver deliver,Failure failure) {
        if(!owner || !work || !deliver || !failure)throw std::invalid_argument("Incomplete tracked database jobs");
        auto result=std::shared_ptr<VerticalTrackedDbJobs>(new VerticalTrackedDbJobs(std::move(strand),registry,std::move(owner),account,std::move(work),std::move(deliver),std::move(failure)));
        std::weak_ptr<VerticalTrackedDbJobs> weak=result;
        result->client_=dispatcher.client([weak](std::uint32_t job){if(auto self=weak.lock())self->run(job);});return result;
    }
    // Used before shutdown mutates producer state; both identity and the
    // serialized executor must match the registry being drained.
    bool bound_to(const Strand& strand,const VerticalSessionRegistry<Session>& registry) const noexcept {
        return &registry_==&registry && strand_==strand;
    }
    bool submit(std::uint32_t job) {
        require_strand();if(!accepting_)return false;return enqueue(job);
    }
    void stop_producers() {require_strand();accepting_=false;}
    // Nonblocking world-strand notification; a lost job or failure intentionally
    // prevents completion. This does not account for other trackers/producers.
    void when_drained(std::function<void()> done) {
        require_strand();if(accepting_ || !done)throw std::logic_error("Drain requires stopped producers and a callback");
        if(outstanding_==0){done();return;}drain_waiters_.push_back(std::move(done));
    }
private:
    VerticalTrackedDbJobs(Strand s,VerticalSessionRegistry<Session>& r,std::shared_ptr<Session> o,std::uint32_t account,Work w,Deliver d,Failure f)
        :strand_(std::move(s)),registry_(r),owner_(std::move(o)),account_(account),work_(std::move(w)),deliver_(std::move(d)),failure_(std::move(f)) {}
    void require_strand() const {if(!strand_.running_in_this_thread())throw std::logic_error("DB job submission outside world strand");}
    // A producer stop does not cancel retries requested by already accepted work.
    bool enqueue(std::uint32_t job) {
        if(job==0 || !registry_.begin_database_work(account_,*owner_))return false;
        ++outstanding_;
        // A throwing post could already have enqueued work; preserve the fence.
        if(!client_->post(job)){(void)registry_.finish_database_work(account_,*owner_);--outstanding_;return false;}
        return true;
    }
    void run(std::uint32_t job) {
        Packets replies;std::vector<std::uint32_t> retry;std::exception_ptr error;
        try {replies=work_(*owner_,job,[&](std::uint32_t next){retry.push_back(next);});}catch(...){error=std::current_exception();}
        auto self=this->shared_from_this();asio::post(strand_,[self,replies=std::move(replies),retry=std::move(retry),error]() mutable {
            if(error) {self->notify(error);return;}
            try {
                for(auto job:retry)if(!self->enqueue(job))throw std::runtime_error("Tracked DB retry rejected");
                self->deliver_(*self->owner_,std::move(replies));
                if(!self->registry_.finish_database_work(self->account_,*self->owner_))throw std::logic_error("Tracked DB completion lost owner");
                --self->outstanding_;self->notify_drained();
            } catch(...) {self->notify(std::current_exception());}
        });
    }
    void notify_drained() {
        if(accepting_ || outstanding_)return;
        auto waiters=std::move(drain_waiters_);drain_waiters_.clear();
        for(auto& waiter:waiters)try{waiter();}catch(...){notify(std::current_exception());}
    }
    void notify(std::exception_ptr error) noexcept {try{failure_(error);}catch(...){}}
    Strand strand_;VerticalSessionRegistry<Session>& registry_;std::shared_ptr<Session> owner_;std::uint32_t account_;
    Work work_;Deliver deliver_;Failure failure_;std::shared_ptr<DbJobClient> client_;bool accepting_=true;std::size_t outstanding_=0;std::vector<std::function<void()>> drain_waiters_;
};
}
