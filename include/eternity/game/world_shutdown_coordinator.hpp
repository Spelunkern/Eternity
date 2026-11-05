#pragma once
#include "eternity/game/world_frame_driver.hpp"
#include "eternity/game/vertical_db_drain_group.hpp"
#include <asio/post.hpp>

namespace eternity::game {
// One instance per world on its shared strand. The caller must first stop
// admission, close sessions, fence unsolicited transport, and retain the
// registry and this coordinator through completion. The trackers must use
// this exact registry. This barrier covers only the supplied tracked DB clients; missing or failed work remains fenced.
template<class Session> class WorldShutdownCoordinator final : public std::enable_shared_from_this<WorldShutdownCoordinator<Session>> {
public:
    using Tracker=VerticalTrackedDbJobs<Session>;
    using Strand=WorldFrameDriver::Strand;
    static std::shared_ptr<WorldShutdownCoordinator> start(Strand strand,std::shared_ptr<WorldFrameDriver> driver,
        VerticalSessionRegistry<Session>& registry,std::vector<std::shared_ptr<Tracker>> trackers,std::function<void()> db_drained) {
        if(!strand.running_in_this_thread()) throw std::logic_error("World shutdown requires its world strand");
        if(!driver || !db_drained) throw std::invalid_argument("World shutdown requires driver and DB drain notification");
        for(const auto& tracker:trackers) if(!tracker) throw std::invalid_argument("World shutdown requires actual DB trackers");
        auto result=std::shared_ptr<WorldShutdownCoordinator>(new WorldShutdownCoordinator(std::move(strand),std::move(driver),registry,std::move(db_drained)));
        result->driver_->stop();
        std::weak_ptr<WorldShutdownCoordinator> weak=result;
        result->group_=VerticalDbDrainGroup<Session>::start(std::move(trackers),[weak]{
            if(auto self=weak.lock()) {
                self->db_drained_=true;
                // Drain groups may complete inline; notify only after start()
                // returns so the owner can retain this coordinator first.
                asio::post(self->strand_,[weak]{if(auto retained=weak.lock()) {auto done=std::move(retained->db_drained_callback_);done();}});
            }
        });
        return result;
    }
    // Call after the drain notification and removal of all registered sessions.
    // False preserves the world if tracked DB work, sessions, leases or dispatch
    // remain. No implicit session release or database shutdown is performed.
    bool try_finish() {
        if(!strand_.running_in_this_thread()) throw std::logic_error("World shutdown requires its world strand");
        if(!db_drained_ || registry_.size()!=0) return false;
        return driver_->shutdown();
    }
private:
    WorldShutdownCoordinator(Strand strand,std::shared_ptr<WorldFrameDriver> driver,VerticalSessionRegistry<Session>& registry,std::function<void()> done)
        :strand_(std::move(strand)),driver_(std::move(driver)),registry_(registry),db_drained_callback_(std::move(done)) {}
    Strand strand_;std::shared_ptr<WorldFrameDriver> driver_;VerticalSessionRegistry<Session>& registry_;
    std::function<void()> db_drained_callback_;bool db_drained_=false;std::shared_ptr<VerticalDbDrainGroup<Session>> group_;
};
}
