#pragma once
#include "eternity/game/vertical_tracked_db_jobs.hpp"

namespace eternity::game {
// Owner retains this handle on the common world strand, after stopping external
// admission and producers. This barrier covers only the supplied trackers, not
// unrelated DB clients, transport messages or zone shutdown. No threads are
// created; completion can run inline for already-idle work. cancel() abandons
// notification and releases tracker references, but does not cancel jobs or
// clear their registry fences.
template<class Session> class VerticalDbDrainGroup final:public std::enable_shared_from_this<VerticalDbDrainGroup<Session>> {
public:
    using Tracker=VerticalTrackedDbJobs<Session>;
    static std::shared_ptr<VerticalDbDrainGroup> start(std::vector<std::shared_ptr<Tracker>> trackers,std::function<void()> done) {
        if(!done)throw std::invalid_argument("Missing DB drain completion");
        for(const auto& tracker:trackers)if(!tracker)throw std::invalid_argument("Missing DB tracker");
        auto group=std::shared_ptr<VerticalDbDrainGroup>(new VerticalDbDrainGroup(std::move(trackers),std::move(done)));
        group->begin();return group;
    }
    void cancel() {cancelled_=true;done_={};trackers_.clear();}
private:
    VerticalDbDrainGroup(std::vector<std::shared_ptr<Tracker>> trackers,std::function<void()> done)
        :trackers_(std::move(trackers)),done_(std::move(done)),remaining_(trackers_.size()){}
    void begin() {
        for(const auto& tracker:trackers_)tracker->stop_producers();
        if(!remaining_){auto done=std::move(done_);done();return;}
        // Copy keeps iteration safe if the final inline callback destroys the
        // owner's handle or requests cancellation.
        auto trackers=trackers_;auto weak=this->weak_from_this();
        for(const auto& tracker:trackers)tracker->when_drained([weak]{if(auto group=weak.lock())group->completed_one();});
    }
    void completed_one() {
        if(cancelled_)return;
        if(--remaining_==0){auto done=std::move(done_);trackers_.clear();done();}
    }
    std::vector<std::shared_ptr<Tracker>> trackers_;std::function<void()> done_;std::size_t remaining_;bool cancelled_=false;
};
}
