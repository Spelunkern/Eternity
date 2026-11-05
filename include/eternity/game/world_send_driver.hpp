#pragma once
#include "eternity/game/world_frame_driver.hpp"
#include <asio/steady_timer.hpp>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <stdexcept>
#include <utility>

namespace eternity::game {
// CWorldSendThread::Run 00406f90, wait/overrun 004070bc..004070eb.
// Root and all required providers run on the supplied world strand.
template<class Root> class WorldSendDriver final : public std::enable_shared_from_this<WorldSendDriver<Root>> {
public:
    using Strand=WorldFrameDriver::Strand;
    using Failure=WorldFrameDriver::Failure;
    static std::shared_ptr<WorldSendDriver> create(Strand strand,std::shared_ptr<Root> root,
        std::uint32_t period,std::function<std::uint32_t()> tick,WorldFrameDriver::OverrunCounter overruns,Failure failure) {
        if(!root || !period || period>0x7fffffffU || !tick || !overruns || !failure)
            throw std::invalid_argument("World send driver requires world, period, clock, counter and failure owner");
        return std::shared_ptr<WorldSendDriver>(new WorldSendDriver(std::move(strand),root,period,std::move(tick),std::move(overruns),std::move(failure)));
    }
    void start() {
        require_strand();if(started_)throw std::logic_error("World send driver already started or stopped");
        started_=true;try{schedule(0);}catch(...){stopped_=true;throw;}
    }
    void stop() {require_strand();started_=true;if(stopped_)return;stopped_=true;timer_.cancel();}
private:
    WorldSendDriver(Strand strand,std::shared_ptr<Root> root,std::uint32_t period,std::function<std::uint32_t()> tick,
        WorldFrameDriver::OverrunCounter overruns,Failure failure)
        :strand_(std::move(strand)),timer_(strand_),root_(root),period_(period),tick_(std::move(tick)),overruns_(std::move(overruns)),failure_(std::move(failure)) {}
    void require_strand()const {if(!strand_.running_in_this_thread())throw std::logic_error("World send driver requires its world strand");}
    void schedule(std::uint32_t delay) {
        timer_.expires_after(std::chrono::milliseconds(delay));std::weak_ptr<WorldSendDriver> weak=this->shared_from_this();
        timer_.async_wait([weak](const asio::error_code& error){if(auto self=weak.lock())self->on_wait(error);});
    }
    void on_wait(const asio::error_code& error) {
        if(stopped_)return;
        try {
            if(error)throw std::system_error(error);
            auto root=root_.lock();if(!root)throw std::logic_error("World send driver lost its world owner");
            const auto start=tick_();const bool keep_running=root->update_world_send(start);
            const auto after=tick_();const std::uint32_t remaining=start+period_-after;
            const auto overrun=tick_();if(overrun-start>250U)overruns_->fetch_add(1,std::memory_order_relaxed);
            if(stopped_)return;if(!keep_running){stop();return;}
            schedule((remaining&0x80000000U)?0U:remaining);
        } catch(...) {stopped_=true;failure_(std::current_exception());}
    }
    Strand strand_;asio::steady_timer timer_;std::weak_ptr<Root> root_;std::uint32_t period_;
    std::function<std::uint32_t()> tick_;WorldFrameDriver::OverrunCounter overruns_;Failure failure_;
    bool started_=false,stopped_=false;
};
}
