#pragma once
#include "eternity/game/world_admission.hpp"
#include "eternity/net/world_server.hpp"
#include <asio/any_io_executor.hpp>
#include <asio/post.hpp>
#include <asio/strand.hpp>
#include <exception>
#include <memory>
#include <stdexcept>

namespace eternity::game {
// All methods except the socket completion run on the world strand. The owner
// retains this object and its admission user, and calls cancel before freeing
// that user. pause_input's completion posts back without blocking either strand.
class VerticalAdmissionHandoff final:public std::enable_shared_from_this<VerticalAdmissionHandoff> {
public:
    using Strand=asio::strand<asio::any_io_executor>;
    VerticalAdmissionHandoff(Strand strand,net::WorldChannel channel,WorldAdmissionUser& user,
        std::function<void()> paused,std::function<void(std::exception_ptr)> failure)
        :strand_(std::move(strand)),channel_(std::move(channel)),user_(user),paused_(std::move(paused)),failure_(std::move(failure)) {
        if(!channel_.pause_input || !channel_.resume_input || !channel_.set_admission_dispatch || !channel_.disconnect || !paused_ || !failure_)
            throw std::invalid_argument("Incomplete admission handoff ports");
    }
    void start() {
        require_strand();if(state_!=State::idle)throw std::logic_error("Admission pause already started");
        state_=State::pausing;auto weak=weak_from_this();if(weak.expired())throw std::logic_error("Admission handoff requires shared ownership");
        channel_.pause_input([weak,strand=strand_](bool active) {asio::post(strand,[weak,active] {if(auto owner=weak.lock())owner->pause_completed(active);});});
    }
    // Call after the world scheduler publishes location 3; socket operations
    // are posted in this order, so queued 0201 is decoded only after dispatch.
    void zone_ready() {
        require_strand();if(state_!=State::paused || user_.location!=3 || !user_.zone)throw std::logic_error("Zone handoff is not ready");
        auto pin=shared_from_this();state_=State::released;channel_.set_admission_dispatch(true);
        if(state_==State::released)channel_.resume_input();
    }
    void cancel() {require_strand();state_=State::cancelled;}
private:
    enum class State {idle,pausing,paused,released,failed,cancelled};
    void require_strand() const {if(!strand_.running_in_this_thread())throw std::logic_error("Admission handoff outside world strand");}
    void pause_completed(bool active) noexcept {
        if(state_!=State::pausing)return;
        if(!active){state_=State::cancelled;return;}
        state_=State::paused;
        try {paused_();} catch(...) {
            if(state_==State::cancelled)return;
            state_=State::failed;try{failure_(std::current_exception());}catch(...){}
            if(state_==State::failed)try{channel_.disconnect();}catch(...){}
        }
    }
    Strand strand_;net::WorldChannel channel_;WorldAdmissionUser& user_;std::function<void()> paused_;
    std::function<void(std::exception_ptr)> failure_;State state_=State::idle;
};
}
