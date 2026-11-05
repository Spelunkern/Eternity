#pragma once
#include "eternity/game/vertical_world.hpp"
#include "eternity/game/world_frame_cadence.hpp"
#include <asio/any_io_executor.hpp>
#include <asio/steady_timer.hpp>
#include <asio/strand.hpp>
#include <atomic>
#include <chrono>
#include <exception>
#include <memory>
#include <stdexcept>

namespace eternity::game {
// DWORD milliseconds from the same steady-clock epoch as world transport.
// All game deadlines in one process must use this same clock basis.
inline std::uint32_t world_frame_tick_ms() noexcept {
    return static_cast<std::uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}
// All methods and callbacks run on the same strand as the world applications.
// The owner retains the driver while running, stops producers before world
// shutdown, and retains the world until shutdown drains. No threads are made.
class WorldFrameDriver final : public std::enable_shared_from_this<WorldFrameDriver> {
public:
    using Strand=asio::strand<asio::any_io_executor>;
    using Update=std::function<bool(VerticalWorld&,std::uint32_t)>;
    using Failure=std::function<void(std::exception_ptr)>;
    using OverrunCounter=std::shared_ptr<std::atomic<std::uint32_t>>;
    static std::shared_ptr<WorldFrameDriver> create(Strand strand,std::shared_ptr<VerticalWorld> world,
        OverrunCounter overruns,Update update,Failure failure) {
        return create(std::move(strand),std::move(world),std::move(overruns),world_frame_tick_ms,std::move(update),std::move(failure));
    }
    static std::shared_ptr<WorldFrameDriver> create(Strand strand,std::shared_ptr<VerticalWorld> world,
        OverrunCounter overruns,std::function<std::uint32_t()> tick,Update update,Failure failure) {
        if(!world || !overruns || !tick || !update || !failure) throw std::invalid_argument("World frame driver requires real world, shared overrun counter, clock, update and failure services");
        return std::shared_ptr<WorldFrameDriver>(new WorldFrameDriver(std::move(strand),std::move(world),std::move(overruns),std::move(tick),std::move(update),std::move(failure)));
    }
    // One-shot lifetime. The original first waits with timeout zero.
    void start() {
        require_strand();if(state_!=State::fresh) throw std::logic_error("World frame driver already started or stopped");
        if(!world_->running()) throw std::logic_error("World must finish startup before its frame driver starts");
        state_=State::running;
        try {schedule(0);} catch(...) {state_=State::stopped;throw;}
    }
    // Stop scheduling only; world/session shutdown is coordinated separately.
    void stop() {
        require_strand();if(state_==State::stopped) return;
        state_=State::stopped;timer_.cancel();
    }
    // Stop external admission/producers and drain sessions before this call.
    // False means a world lease or dispatch remains: release it and retry on
    // the same strand. No new timer work is scheduled after the first call.
    bool shutdown() {
        require_strand();stop();return world_->shutdown();
    }
    std::uint32_t overruns() const {require_strand();return overruns_->load(std::memory_order_relaxed);}
private:
    enum class State {fresh,running,stopped};
    WorldFrameDriver(Strand strand,std::shared_ptr<VerticalWorld> world,OverrunCounter overruns,std::function<std::uint32_t()> tick,Update update,Failure failure)
        :strand_(std::move(strand)),timer_(strand_),world_(std::move(world)),overruns_(std::move(overruns)),tick_(std::move(tick)),update_(std::move(update)),failure_(std::move(failure)) {}
    void require_strand() const {if(!strand_.running_in_this_thread()) throw std::logic_error("World frame driver requires its world strand");}
    void schedule(std::uint32_t delay) {
        timer_.expires_after(std::chrono::milliseconds(delay));
        std::weak_ptr<WorldFrameDriver> weak=shared_from_this();
        timer_.async_wait([weak](const asio::error_code& error){if(auto self=weak.lock()) self->on_wait(error);});
    }
    void on_wait(const asio::error_code& error) {
        if(state_!=State::running) return;
        try {
            if(error) throw std::system_error(error);
            // ps_game.exe CWorldThread::Run 00407330, 0040734f..00407380.
            const auto start_tick=tick_();bool keep_running=false;
            world_->serialized_update([&](VerticalWorld& world){keep_running=update_(world,start_tick);});
            const auto after_update=tick_();const auto overrun_tick=tick_();
            const auto next=world_frame_cadence(start_tick,after_update,overrun_tick);
            if(next.overrun) overruns_->fetch_add(1,std::memory_order_relaxed);
            if(state_!=State::running) return;
            if(!keep_running) {stop();return;}
            schedule(next.wait_ms);
        } catch(...) {
            const auto error=std::current_exception();state_=State::stopped;
            failure_(error);
        }
    }
    Strand strand_;asio::steady_timer timer_;std::shared_ptr<VerticalWorld> world_;OverrunCounter overruns_;
    std::function<std::uint32_t()> tick_;Update update_;Failure failure_;
    State state_=State::fresh;
};
}
