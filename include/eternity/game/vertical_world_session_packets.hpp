#pragma once
#include "eternity/game/vertical_world_session.hpp"
#include <deque>

namespace eternity::game {
// Retained per-session input FIFO for the world/zone scheduler. The socket
// owner enqueues on the world strand; the admission scheduler calls update for
// that same session in its appropriate live-list pass. A location-3 0201 uses
// the real admission handler; location-4 packets require the real gameplay
// dispatcher. Location 2 consumes/ignores packets as the recovered publication
// gap does. This is not a roster/selection packet handler; other locations fail.
// The count is captured at update start, matching ps_game.exe CUser::UpdatePackets
// 00466cc0 / FIFO Push 004e0740 / Pop 004e07a0 and PacketProcessing 00466da0.
// Limits are native transport bounds supplied by the runtime, not recovered
// original protocol constants. False from enqueue requires a real close policy.
class VerticalWorldSessionPackets final:public std::enable_shared_from_this<VerticalWorldSessionPackets> {
public:
    using Dispatch=std::function<void(VerticalWorldSession&,const net::Packet&)>;
    static std::shared_ptr<VerticalWorldSessionPackets> create(VerticalWorldSession::Strand strand,
        std::shared_ptr<VerticalWorldSession> owner,std::size_t max_packets,std::size_t max_bytes,Dispatch dispatch) {
        if(!strand.running_in_this_thread() || !owner || !max_packets || !max_bytes || !dispatch)
            throw std::invalid_argument("Incomplete session packet queue");
        return std::shared_ptr<VerticalWorldSessionPackets>(new VerticalWorldSessionPackets(std::move(strand),std::move(owner),max_packets,max_bytes,std::move(dispatch)));
    }
    bool enqueue(net::Packet packet) {
        require_strand();auto owner=owner_.lock();
        if(stopped_ || !owner || (owner->closing() || owner->failed()))return false;
        if(packet.size()<2)throw std::invalid_argument("Malformed queued packet");
        if(queue_.size()>=max_packets_ || packet.size()>max_bytes_-bytes_)return false;
        const auto size=packet.size();queue_.push_back(std::move(packet));bytes_+=size;return true;
    }
    void update() {
        require_strand();if(stopped_)return;if(updating_)throw std::logic_error("Reentrant session packet update");
        auto self=shared_from_this();auto owner=owner_.lock();if(!owner)throw std::logic_error("Packet queue lost session");
        owner->begin_admission_pass();updating_=true;const auto count=queue_.size();std::exception_ptr error;
        try {
            for(std::size_t i=0;i<count && !stopped_ && !owner->closing() && !owner->failed() && !queue_.empty();++i) {
                const auto location=owner->admission().location;
                if(location!=2 && location!=3 && location!=4)throw std::logic_error("Packet queue cannot handle this admission location");
                if(location==4 && gameplay_suspended_)break;
                auto packet=std::move(queue_.front());queue_.pop_front();bytes_-=packet.size();
                if(location==3)(void)owner->receive_zone_wait(packet);
                else if(location==4)dispatch_(*owner,packet);
            }
        } catch(...) {error=std::current_exception();}
        updating_=false;try {owner->end_admission_pass();}catch(...) {if(!error)error=std::current_exception();}
        if(error)std::rethrow_exception(error);
    }
    // Stop producers and discard queued packets before releasing the session.
    void stop() {require_strand();stopped_=true;queue_.clear();bytes_=0;}
    void suspend_gameplay() {require_strand();if(stopped_ || gameplay_suspended_)throw std::logic_error("Gameplay queue cannot suspend");gameplay_suspended_=true;}
    void resume_gameplay() {require_strand();if(stopped_ || !gameplay_suspended_)throw std::logic_error("Gameplay queue cannot resume");gameplay_suspended_=false;}
    bool gameplay_suspended() const {require_strand();return gameplay_suspended_;}
    std::size_t pending() const {require_strand();return queue_.size();}
private:
    VerticalWorldSessionPackets(VerticalWorldSession::Strand strand,std::shared_ptr<VerticalWorldSession> owner,
        std::size_t max_packets,std::size_t max_bytes,Dispatch dispatch)
        :strand_(std::move(strand)),owner_(std::move(owner)),max_packets_(max_packets),max_bytes_(max_bytes),dispatch_(std::move(dispatch)){}
    void require_strand() const {if(!strand_.running_in_this_thread())throw std::logic_error("Session packet queue outside world strand");}
    VerticalWorldSession::Strand strand_;std::weak_ptr<VerticalWorldSession> owner_;std::size_t max_packets_,max_bytes_,bytes_=0;
    Dispatch dispatch_;std::deque<net::Packet> queue_;bool stopped_=false,updating_=false,gameplay_suspended_=false;
};
}
