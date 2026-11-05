#pragma once
#include "eternity/game/world_frame_driver.hpp"
#include <asio/post.hpp>
#include <asio/steady_timer.hpp>
#include <chrono>
#include <exception>
#include <functional>
#include <memory>
#include <stdexcept>

namespace eternity::game {
// Retained by the process shutdown owner until completion or explicit stop.
// Root, clock and completion must be the real owners on the world strand.
template<class Root> class WorldShutdownPoller final : public std::enable_shared_from_this<WorldShutdownPoller<Root>> {
public:
    using Strand=WorldFrameDriver::Strand;
    using Completion=std::function<void(std::exception_ptr)>;
    static std::shared_ptr<WorldShutdownPoller> start(Strand strand,std::shared_ptr<Root> root,
        std::chrono::milliseconds interval,std::function<std::uint32_t()> tick,Completion completion) {
        if(!strand.running_in_this_thread()) throw std::logic_error("World shutdown poller requires its world strand");
        if(!root || interval.count()<=0 || !tick || !completion) throw std::invalid_argument("World shutdown poller requires real root, clock, interval and completion");
        auto result=std::shared_ptr<WorldShutdownPoller>(new WorldShutdownPoller(std::move(strand),std::move(root),interval,std::move(tick),std::move(completion)));
        result->schedule(std::chrono::milliseconds(0));return result;
    }
    // Stops only polling; it does not claim the world or its providers drained.
    void stop() {
        require_strand();if(stopped_) return;stopped_=true;timer_.cancel();
    }
private:
    WorldShutdownPoller(Strand strand,std::shared_ptr<Root> root,std::chrono::milliseconds interval,
        std::function<std::uint32_t()> tick,Completion completion)
        :strand_(std::move(strand)),timer_(strand_),root_(std::move(root)),interval_(interval),tick_(std::move(tick)),completion_(std::move(completion)) {}
    void require_strand() const {if(!strand_.running_in_this_thread()) throw std::logic_error("World shutdown poller requires its world strand");}
    void schedule(std::chrono::milliseconds delay) {
        timer_.expires_after(delay);std::weak_ptr<WorldShutdownPoller> weak=this->shared_from_this();
        timer_.async_wait([weak](const asio::error_code& error){if(auto self=weak.lock())self->on_wait(error);});
    }
    void on_wait(const asio::error_code& error) {
        if(stopped_) return;
        try {
            if(error) throw std::system_error(error);
            if(!root_->poll_shutdown(tick_())) {schedule(interval_);return;}
        } catch(...) {finish(std::current_exception());return;}
        finish({});
    }
    void finish(std::exception_ptr error) {
        stopped_=true;auto completion=std::move(completion_);completion(std::move(error));
    }
    Strand strand_;asio::steady_timer timer_;std::shared_ptr<Root> root_;std::chrono::milliseconds interval_;
    std::function<std::uint32_t()> tick_;Completion completion_;bool stopped_=false;
};
}
