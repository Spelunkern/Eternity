#pragma once
#include <atomic>
#include <cstddef>
#include <memory>
#include <utility>

namespace eternity::net::detail {
// Accounts for framed bytes before socket-executor posting and until write
// completion, including messages abandoned during close. Not a legacy limit.
class OutboundBudget {
    struct State {
        explicit State(std::size_t maximum):limit(maximum) {}
        const std::size_t limit;
        std::atomic_size_t used{0};
        std::atomic_size_t frames{0};
    };
public:
    class Reservation {
    public:
        Reservation(std::shared_ptr<State> state,std::size_t bytes):state_(std::move(state)),bytes_(bytes) {state_->frames.fetch_add(1,std::memory_order_relaxed);}
        ~Reservation() {state_->frames.fetch_sub(1,std::memory_order_relaxed);state_->used.fetch_sub(bytes_,std::memory_order_relaxed);}
        Reservation(const Reservation&)=delete;
        Reservation& operator=(const Reservation&)=delete;
    private:
        std::shared_ptr<State> state_;
        std::size_t bytes_;
    };
    explicit OutboundBudget(std::size_t limit):state_(std::make_shared<State>(limit)) {}
    std::shared_ptr<Reservation> reserve(std::size_t bytes) const {
        auto used=state_->used.load(std::memory_order_relaxed);
        do {
            if(bytes>state_->limit || used>state_->limit-bytes) return {};
        } while(!state_->used.compare_exchange_weak(used,used+bytes,std::memory_order_relaxed));
        try {return std::make_shared<Reservation>(state_,bytes);}
        catch(...) {state_->used.fetch_sub(bytes,std::memory_order_relaxed);throw;}
    }
    std::size_t used() const noexcept {return state_->used.load(std::memory_order_relaxed);}
    std::size_t frames() const noexcept {return state_->frames.load(std::memory_order_relaxed);}
    std::size_t limit() const noexcept {return state_->limit;}
private:
    std::shared_ptr<State> state_;
};
}
