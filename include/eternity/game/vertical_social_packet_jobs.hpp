#pragma once
#include "eternity/game/vertical_social_session.hpp"
#include "eternity/game/vertical_tracked_db_jobs.hpp"
#include <deque>
#include <mutex>

namespace eternity::game {
template<class Session,class Database,class SaveJobs> class VerticalSocialSaveFence;
// Serialized world producer / DB-worker consumer for one retained account.
// PacketUserFriend 00406930 produces no reply. One tracked worker drains this
// FIFO before another can start; this preserves add/memo/delete ordering even
// when DbJobDispatcher runs multiple workers. The session owns the social owner
// and retains this object until stop_and_drain completes.
template<class Session,class Database> class VerticalSocialPacketJobs final:public std::enable_shared_from_this<VerticalSocialPacketJobs<Session,Database>> {
public:
    using Social=VerticalSocialSession<Session,Database>;
    using Tracker=VerticalTrackedDbJobs<Session>;
    using Strand=typename Tracker::Strand;
    using Failure=typename Tracker::Failure;
    static std::shared_ptr<VerticalSocialPacketJobs> create(Strand strand,VerticalSessionRegistry<Session>& registry,
        std::shared_ptr<Session> session,Social& social,DbJobDispatcher& dispatcher,Failure failure) {
        if(!strand.running_in_this_thread() || !session || registry.find_account(social.account_id())!=session || !social.owns_session(*session) || !failure)
            throw std::invalid_argument("Incomplete tracked social packet services");
        if(!social.claim_packet_jobs())throw std::logic_error("Social packet jobs already bound");
        try {
        auto result=std::shared_ptr<VerticalSocialPacketJobs>(new VerticalSocialPacketJobs(strand,*session,social.account_id(),social,std::move(failure)));
        std::weak_ptr<VerticalSocialPacketJobs> weak=result;auto queue=result->queue_;
        result->tracker_=Tracker::create(strand,registry,std::move(session),result->account_,dispatcher,
            [queue](Session&,std::uint32_t,const typename Tracker::Retry&)->typename Tracker::Packets {
                for(;;) {
                    typename Queue::Item item;
                    {std::lock_guard lock(queue->mutex);if(queue->packets.empty())break;item=std::move(queue->packets.front());queue->packets.pop_front();}
                    if(item.barrier) {std::lock_guard lock(queue->mutex);queue->ready=std::move(item.barrier);break;}
                    if(queue->social->apply_db(item.packet)!=EntryCatalogUpdate::applied)throw std::runtime_error("Tracked social DB packet rejected");
                }
                return {};
            },
            [weak](Session&,typename Tracker::Packets replies){
                auto self=weak.lock();if(!self)throw std::logic_error("Tracked social packet owner lost");
                if(!replies.empty())throw std::logic_error("Unexpected social DB reply");
                self->finished_batch();
            },
            [weak](std::exception_ptr error){if(auto self=weak.lock()){self->failed_=true;self->accepting_=false;self->failure_(error);}});
        return result;
        } catch(...) {social.release_packet_jobs();throw;}
    }
    bool owns_session(const Session& session) const noexcept {return session_==&session;}
    bool owns_social(const Social& social) const noexcept {return queue_->social==&social;}
    bool uses_save_mutex(const std::recursive_mutex& mutex) const noexcept {return queue_->social->uses_save_mutex(mutex);}
    std::uint32_t account_id() const noexcept {return account_;}
    bool on_world_strand() const noexcept {return strand_.running_in_this_thread();}
    bool submit(net::Packet packet) {
        require_strand();if(!accepting_ || failed_)return false;
        if(packet.size()<6 || packet[1]!=0x0c || (packet[0]!=2 && packet[0]!=3 && packet[0]!=4 && packet[0]!=6 && packet[0]!=7 && packet[0]!=8))
            throw std::invalid_argument("Invalid social DB packet envelope");
        std::uint32_t id=0;for(unsigned i=0;i<4;++i)id|=std::uint32_t{packet[2+i]}<<(8*i);
        if(id!=account_)throw std::invalid_argument("Foreign social DB packet");
        return enqueue({std::move(packet),{}});
    }
    // Submit the real save job on the world strand only after every social
    // packet accepted before this marker has completed its replica mutation.
    // Rejected or throwing save submission leaves the tracked work fenced.
    bool before_save(std::function<bool()> submit_save) {
        require_strand();if(!accepting_ || failed_)return false;
        if(!submit_save)throw std::invalid_argument("Missing social save submission");
        return enqueue({{},[submit_save=std::move(submit_save)] {if(!submit_save())throw std::runtime_error("Social-ordered save was not accepted");}});
    }
    void stop_and_drain(std::function<void()> done) {start_drain(false,std::move(done));}
private:
    template<class S,class D,class J> friend class VerticalSocialSaveFence;
    void start_drain(bool hold_claim,std::function<void()> done) {
        require_strand();if(!done || draining_)throw std::logic_error("Invalid social packet drain");
        accepting_=false;draining_=true;hold_claim_=hold_claim;done_=std::move(done);finish_if_drained();
    }
    void release_held_claim() {
        require_strand();if(!drained_ || !hold_claim_ || claim_released_)throw std::logic_error("Invalid social packet claim release");
        queue_->social->release_packet_jobs();claim_released_=true;
    }
    struct Queue {
        struct Item {net::Packet packet;std::function<void()> barrier;};
        std::mutex mutex;std::deque<Item> packets;std::function<void()> ready;Social* social;
        explicit Queue(Social& value):social(&value){}
    };
    VerticalSocialPacketJobs(Strand strand,Session& session,std::uint32_t account,Social& social,Failure failure)
        :strand_(std::move(strand)),session_(&session),account_(account),queue_(std::make_shared<Queue>(social)),failure_(std::move(failure)) {}
    void require_strand() const {if(!strand_.running_in_this_thread())throw std::logic_error("Social packet submission outside world strand");}
    bool queued() const {std::lock_guard lock(queue_->mutex);return !queue_->packets.empty();}
    bool enqueue(typename Queue::Item item) {
        {std::lock_guard lock(queue_->mutex);queue_->packets.push_back(std::move(item));}
        if(active_)return true;
        active_=true;
        // A throwing post may already have queued work: preserve its fence.
        if(!tracker_->submit(1)) {active_=false;std::lock_guard lock(queue_->mutex);queue_->packets.pop_back();return false;}
        return true;
    }
    void finished_batch() {
        require_strand();std::function<void()> barrier;
        {std::lock_guard lock(queue_->mutex);barrier=std::move(queue_->ready);}
        // Keep active while invoking: a reentrant submit must queue behind the
        // marker and cannot start a second worker for this account.
        if(barrier)barrier();
        active_=false;
        if(queued()) {active_=true;if(!tracker_->submit(1))throw std::runtime_error("Tracked social DB continuation rejected");}
        finish_if_drained();
    }
    void finish_if_drained() {
        if(!draining_ || active_ || queued() || failed_)return;
        tracker_->stop_producers();auto self=this->shared_from_this();
        tracker_->when_drained([self=std::move(self),done=std::move(done_)]() mutable {
            self->drained_=true;
            if(!self->hold_claim_){self->queue_->social->release_packet_jobs();self->claim_released_=true;}
            done();
        });
    }
    Strand strand_;Session* session_;std::uint32_t account_;std::shared_ptr<Queue> queue_;Failure failure_;
    std::shared_ptr<Tracker> tracker_;std::function<void()> done_;bool accepting_=true,active_=false,draining_=false,failed_=false,hold_claim_=false,drained_=false,claim_released_=false;
};

// World-strand producer; it resolves the actual target account's tracked owner.
// A missing account, missing jobs, rejected submission or malformed envelope is
// an error, never an acknowledged success.
template<class Session,class Database> class VerticalSocialPacketRouter final {
public:
    using Jobs=VerticalSocialPacketJobs<Session,Database>;
    using JobsLookup=std::function<Jobs*(Session&)>;
    using Strand=typename Jobs::Strand;
    VerticalSocialPacketRouter(Strand strand,VerticalSessionRegistry<Session>& registry,JobsLookup lookup):strand_(std::move(strand)),registry_(registry),lookup_(std::move(lookup)) {
        if(!lookup_)throw std::invalid_argument("Missing social packet jobs lookup");
    }
    void send(net::Packet packet) {
        if(!strand_.running_in_this_thread())throw std::logic_error("Social packet routing outside world strand");
        if(packet.size()<6)throw std::invalid_argument("Short social DB packet");
        std::uint32_t account=0;for(unsigned i=0;i<4;++i)account|=std::uint32_t{packet[2+i]}<<(8*i);
        auto session=registry_.find_account(account);if(!session)throw std::runtime_error("Social DB account is not retained");
        auto* jobs=lookup_(*session);if(!jobs || !jobs->owns_session(*session) || !jobs->submit(std::move(packet)))throw std::runtime_error("Social DB packet was not accepted");
    }
private:
    Strand strand_;VerticalSessionRegistry<Session>& registry_;JobsLookup lookup_;
};
}
