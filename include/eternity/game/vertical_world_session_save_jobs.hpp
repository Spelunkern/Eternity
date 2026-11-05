#pragma once
#include "eternity/game/vertical_save_job_bridge.hpp"
#include "eternity/game/vertical_world_session_ack.hpp"

namespace eternity::game {
// Save bridge, tracker and router retain the actual session throughout accepted
// work. All public methods and reply callbacks run on the world strand. The
// bridge/store/mutex and any dependencies referenced by them must remain alive
// until drain; callers must stop every other producer and fence unsolicited
// transport traffic separately. Only the 0302 reply is routed here: all other
// DB-agent replies require the supplied real dispatcher.
template<class Store=CharacterSaveStore> class VerticalWorldSessionSaveJobs final {
public:
    using Bridge=VerticalSaveJobBridge<Store>;
    using Tracker=VerticalTrackedDbJobs<VerticalWorldSession>;
    using Other=std::function<void(VerticalWorldSession&,net::Packet)>;
    using Failure=typename Tracker::Failure;
    static std::shared_ptr<VerticalWorldSessionSaveJobs> create(VerticalWorldSession::Strand strand,
        VerticalWorldSession::Registry& registry,std::shared_ptr<VerticalWorldSession> session,DbJobDispatcher& dispatcher,
        std::shared_ptr<Bridge> bridge,std::shared_ptr<VerticalWorldSessionAckRouter> router,Other other,Failure failure) {
        if(!strand.running_in_this_thread() || !session || registry.find_account(session->account_id())!=session ||
           !session->character() || !bridge || !router || !router->matches(strand,registry) || !other || !failure)
            throw std::invalid_argument("Incomplete world session save jobs");
        const auto account=session->account_id();
        auto tracker=Tracker::create(strand,registry,std::move(session),account,dispatcher,
            [bridge](VerticalWorldSession&,std::uint32_t job,const typename Tracker::Retry& retry){return bridge->dispatch_collect(job,retry);},
            [router,other=std::move(other),account](VerticalWorldSession& owner,typename Tracker::Packets packets) {
                for(auto& packet:packets) {
                    if(packet.size()>=2 && packet[0]==2 && packet[1]==3) {
                        if(packet.size()!=6)throw std::invalid_argument("Malformed DB disconnect acknowledgment");
                        std::uint32_t user=0;for(unsigned i=0;i<4;++i)user|=std::uint32_t(packet[2+i])<<(i*8);
                        if(user!=account || !router->receive(packet))throw std::logic_error("Foreign DB disconnect acknowledgment");
                    } else other(owner,std::move(packet));
                }
            },std::move(failure));
        return std::shared_ptr<VerticalWorldSessionSaveJobs>(new VerticalWorldSessionSaveJobs(std::move(bridge),std::move(tracker)));
    }
    bool submit(std::uint32_t job) {return tracker_->submit(job);}
    // Stop only after the final externally produced job has been accepted.
    // Retries already requested by accepted work remain fenced and are drained.
    void stop_and_drain(std::function<void()> done) {auto tracker=tracker_;tracker->stop_producers();tracker->when_drained(std::move(done));}
    Bridge& bridge() noexcept {return *bridge_;}
private:
    VerticalWorldSessionSaveJobs(std::shared_ptr<Bridge> bridge,std::shared_ptr<Tracker> tracker):bridge_(std::move(bridge)),tracker_(std::move(tracker)){}
    std::shared_ptr<Bridge> bridge_;std::shared_ptr<Tracker> tracker_;
};
}
