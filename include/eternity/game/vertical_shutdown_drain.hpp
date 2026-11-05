#pragma once
#include "eternity/game/vertical_db_drain_group.hpp"
#include <set>

namespace eternity::game {
// Join actual retained-session release and the supplied tracked DB clients.
// Call only after transport close, session-close submission, final-save job
// submission and all other relevant producer/unsolicited-traffic fences. The
// caller retains this object, registry, trackers and both executors until the
// notification, and releases this handle on that strand. It does not close sessions, perform SQL, release reservations,
// stop the world driver or prove zone/world shutdown. All methods use one world
// strand; completion must not throw. Failed or stranded work remains fenced.
template<class Session> class VerticalShutdownDrain final:public std::enable_shared_from_this<VerticalShutdownDrain<Session>> {
public:
    using Tracker=VerticalTrackedDbJobs<Session>;
    using Registry=VerticalSessionRegistry<Session>;
    using Strand=typename Tracker::Strand;
    static std::shared_ptr<VerticalShutdownDrain> create(Strand strand,Registry& registry,
        std::vector<std::shared_ptr<Tracker>> trackers,std::function<void()> done) {
        if(!strand.running_in_this_thread() || !done || registry.admission_closed())
            throw std::invalid_argument("Invalid world shutdown drain");
        std::set<const Tracker*> unique;
        for(const auto& tracker:trackers)
            if(!tracker || !tracker->bound_to(strand,registry) || !unique.insert(tracker.get()).second)
                throw std::invalid_argument("Shutdown tracker has wrong owner, strand or duplicate identity");
        return std::shared_ptr<VerticalShutdownDrain>(new VerticalShutdownDrain(std::move(strand),registry,std::move(trackers),std::move(done)));
    }
    // Retain the concrete coordinator session owner and obtain its actual
    // registry on the world strand. Its other referenced dependencies still
    // require their own process-level lifetime and drain barriers.
    template<class Owner>
    static std::shared_ptr<VerticalShutdownDrain> create_for_owner(Strand strand,std::shared_ptr<Owner> owner,
        std::vector<std::shared_ptr<Tracker>> trackers,std::function<void()> done) {
        if(!owner || !strand.running_in_this_thread())throw std::invalid_argument("World session owner requires its strand");
        auto& registry=owner->registry();
        auto result=create(std::move(strand),registry,std::move(trackers),std::move(done));
        result->lifetime_=std::move(owner);return result;
    }
    // WorldRoot exposes the corresponding registry as sessions(). The root is
    // pinned, but this drain does not start or finish its world shutdown.
    template<class Root>
    static std::shared_ptr<VerticalShutdownDrain> create_for_root(Strand strand,std::shared_ptr<Root> root,
        std::vector<std::shared_ptr<Tracker>> trackers,std::function<void()> done) {
        if(!root || !strand.running_in_this_thread())throw std::invalid_argument("World root requires its strand");
        auto& registry=root->sessions();
        auto result=create(std::move(strand),registry,std::move(trackers),std::move(done));
        result->lifetime_=std::move(root);return result;
    }
    // Retain this handle before start. False means setup failed, potentially
    // after admission closed or producers stopped; do not infer completion.
    bool start() {
        require_strand();if(started_)throw std::logic_error("World shutdown drain already started");
        started_=true;auto weak=this->weak_from_this();
        try {
            registry_.close_admission_and_wait(strand_,[weak]{if(auto self=weak.lock())self->sessions_drained();});
            group_=VerticalDbDrainGroup<Session>::start(std::move(trackers_),[weak]{if(auto self=weak.lock())self->database_drained();});
            return true;
        } catch(...) {error_=std::current_exception();return false;}
    }
    // Setup and session-notification posting errors only. DB work/delivery
    // failures go to each real tracker's failure port and leave this pending.
    std::exception_ptr error() const {
        require_strand();if(error_)return error_;
        return started_ && registry_.admission_closed()?registry_.drain_notification_error():std::exception_ptr{};
    }
    void retry_session_notification() {require_strand();if(!started_ || error_)throw std::logic_error("Shutdown drain cannot retry");registry_.retry_drain_notification();}
private:
    VerticalShutdownDrain(Strand strand,Registry& registry,std::vector<std::shared_ptr<Tracker>> trackers,std::function<void()> done)
        :strand_(std::move(strand)),registry_(registry),trackers_(std::move(trackers)),done_(std::move(done)) {}
    void require_strand() const {if(!strand_.running_in_this_thread())throw std::logic_error("World shutdown drain requires the world strand");}
    void sessions_drained() {sessions_done_=true;complete_if_ready();}
    void database_drained() {database_done_=true;complete_if_ready();}
    void complete_if_ready() {
        if(error_ || !sessions_done_ || !database_done_ || !done_)return;
        auto done=std::move(done_);group_.reset();done();
    }
    Strand strand_;Registry& registry_;std::vector<std::shared_ptr<Tracker>> trackers_;
    // Destroy the optional owner pin after this handle's tracker group and
    // callback. Dropping the handle early still does not drain external work.
    std::shared_ptr<void> lifetime_;
    std::function<void()> done_;std::shared_ptr<VerticalDbDrainGroup<Session>> group_;
    std::exception_ptr error_;bool started_=false,sessions_done_=false,database_done_=false;
};
}
