#pragma once
#include "eternity/game/vertical_social_packet_jobs.hpp"

namespace eternity::game {
// Compose the native social queue with the actual per-session save-job owner.
// The caller serializes all methods on the world strand, binds both owners to
// the same account and save mutex, stops all other producers before draining,
// and retains this object and the session through completion. The save owner
// must expose submit(job) and stop_and_drain(done), as does
// VerticalWorldSessionSaveJobs. Original save contracts: ps_dbAgent.exe
// DBFriend::SaveFriend 004261e0 / SaveBlock 004266e0.
template<class Session,class Database,class SaveJobs> class VerticalSocialSaveFence final {
public:
    using PacketJobs=VerticalSocialPacketJobs<Session,Database>;
    using Registry=VerticalSessionRegistry<Session>;
    using Failure=std::function<void(std::exception_ptr)>;
    using SaveLookup=std::function<std::shared_ptr<SaveJobs>(Session&)>;
    VerticalSocialSaveFence(const std::shared_ptr<Session>& session,std::shared_ptr<PacketJobs> packets,Registry& registry,SaveLookup save_for,Failure failure)
        :session_(session),packets_(std::move(packets)),registry_(registry),failure_(std::move(failure)) {
        if(!session_ || !packets_ || !packets_->owns_session(*session_) || !save_for || !failure_)
            throw std::invalid_argument("Incomplete social-ordered save owners");
        if(!packets_->on_world_strand())throw std::logic_error("Social-ordered save binding outside world strand");
        if(registry_.find_account(packets_->account_id())!=session_)throw std::invalid_argument("Social save session is not retained");
        saves_=save_for(*session_);
        if(!saves_)throw std::invalid_argument("Missing retained save owner");
    }
    bool submit(std::uint32_t job) {
        if(!packets_->on_world_strand())throw std::logic_error("Social-ordered save outside world strand");
        if(stopping_)return false;
        if(!job)throw std::invalid_argument("Invalid social-ordered save job");
        return packets_->before_save([saves=saves_,job]{return saves->submit(job);});
    }
    // Stop social production first; after every accepted social packet/marker
    // has drained, stop and drain the actual save owner before calling done.
    // An error reports failure and deliberately does not call done.
    void stop_and_drain(std::function<void()> done) {
        if(!packets_->on_world_strand())throw std::logic_error("Social-ordered save drain outside world strand");
        if(stopping_ || !done)throw std::logic_error("Invalid social-ordered save drain");
        if(!registry_.begin_database_work(packets_->account_id(),*session_))throw std::runtime_error("Social save session fence rejected");
        stopping_=true;auto packets=packets_;auto saves=saves_;auto failure=failure_;auto session=session_;auto* registry=&registry_;
        packets_->start_drain(true,[packets=std::move(packets),saves=std::move(saves),failure=std::move(failure),session=std::move(session),registry,done=std::move(done)]() mutable {
            try {
                saves->stop_and_drain([packets,session,registry,failure,done=std::move(done)]() mutable {
                    try {
                        packets->release_held_claim();
                        if(!registry->finish_database_work(packets->account_id(),*session))throw std::logic_error("Social save session fence lost");
                        done();
                    } catch(...) {failure(std::current_exception());}
                });
            } catch(...) {failure(std::current_exception());}
        });
    }
private:
    std::shared_ptr<Session> session_;std::shared_ptr<PacketJobs> packets_;Registry& registry_;std::shared_ptr<SaveJobs> saves_;Failure failure_;bool stopping_=false;
};
}
