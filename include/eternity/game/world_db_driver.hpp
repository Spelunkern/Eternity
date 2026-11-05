#pragma once
#include "eternity/game/world_db_update.hpp"
#include "eternity/game/world_frame_cadence.hpp"
#include "eternity/game/world_frame_driver.hpp"
#include <asio/steady_timer.hpp>
#include <atomic>
#include <chrono>
#include <exception>
#include <functional>
#include <memory>
#include <stdexcept>

namespace eternity::game {
// CWorldDBThread::CWorldDBThread 00406de0, OnUpdate 00406e20 and
// SThreadTTerm::Run 004d9e10. Caller retains/stops this owner on its strand.
template<class Update> class WorldDbDriver final : public std::enable_shared_from_this<WorldDbDriver<Update>> {
public:
    using Strand=WorldFrameDriver::Strand;
    using Failure=std::function<void(std::exception_ptr)>;
    static std::shared_ptr<WorldDbDriver> create(Strand strand,std::shared_ptr<Update> update,
        std::shared_ptr<std::atomic<std::uint32_t>> heartbeat,std::shared_ptr<std::atomic<std::uint32_t>> overruns,
        std::function<std::uint32_t()> tick,Failure failure) {
        if(!update || !heartbeat || !overruns || !tick || !failure) throw std::invalid_argument("World DB driver requires actual update, clock, counters and failure owner");
        return std::shared_ptr<WorldDbDriver>(new WorldDbDriver(std::move(strand),std::move(update),std::move(heartbeat),std::move(overruns),std::move(tick),std::move(failure)));
    }
    void start() {
        require_strand();if(started_) throw std::logic_error("World DB driver already started or stopped");
        started_=true;heartbeat_->store(tick_(),std::memory_order_relaxed);
        try {schedule(0);} catch(...) {stopped_=true;throw;}
    }
    void stop() {require_strand();started_=true;if(stopped_)return;stopped_=true;timer_.cancel();}
private:
    WorldDbDriver(Strand strand,std::shared_ptr<Update> update,std::shared_ptr<std::atomic<std::uint32_t>> heartbeat,
        std::shared_ptr<std::atomic<std::uint32_t>> overruns,std::function<std::uint32_t()> tick,Failure failure)
        :strand_(std::move(strand)),timer_(strand_),update_(std::move(update)),heartbeat_(std::move(heartbeat)),overruns_(std::move(overruns)),tick_(std::move(tick)),failure_(std::move(failure)) {}
    void require_strand() const {if(!strand_.running_in_this_thread()) throw std::logic_error("World DB driver requires its world strand");}
    void schedule(std::uint32_t delay) {
        timer_.expires_after(std::chrono::milliseconds(delay));std::weak_ptr<WorldDbDriver> weak=this->shared_from_this();
        timer_.async_wait([weak](const asio::error_code& error){if(auto self=weak.lock())self->on_wait(error);});
    }
    void on_wait(const asio::error_code& error) {
        if(stopped_) return;
        try {
            if(error) throw std::system_error(error);
            const auto start=tick_();
            if(update_->update(start)==WorldDbUpdateResult::interrupted) {stopped_=true;return;}
            const auto after_update=tick_();
            if(after_update-start>250U) overruns_->fetch_add(1,std::memory_order_relaxed);
            heartbeat_->store(start,std::memory_order_relaxed);
            const auto after_heartbeat=tick_();
            if(!stopped_) schedule(world_frame_cadence(start,after_heartbeat,after_heartbeat).wait_ms);
        } catch(...) {stopped_=true;failure_(std::current_exception());}
    }
    Strand strand_;asio::steady_timer timer_;std::shared_ptr<Update> update_;
    std::shared_ptr<std::atomic<std::uint32_t>> heartbeat_,overruns_;std::function<std::uint32_t()> tick_;Failure failure_;
    bool started_=false,stopped_=false;
};
}
