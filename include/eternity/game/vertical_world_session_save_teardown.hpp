#pragma once
#include "eternity/game/vertical_world_session_preparation.hpp"

namespace eternity::game {
// Runs after the real disconnect path has stopped external producers and
// completed world/zone cleanup. It drains the session's tracked save jobs,
// then releases the preparation's binding on the DB executor. It does not
// fence unrelated trackers, unsolicited transport packets, or external SQL
// resource aliases. The caller must keep those barriers and real services alive.
// ps_game.exe CWorld::LeaveUser 0040ca20 and DB-agent 0302 00408d30..00408dfd.
template<class Resource,class Store=CharacterSaveStore> class VerticalWorldSessionSaveTeardown final:public std::enable_shared_from_this<VerticalWorldSessionSaveTeardown<Resource,Store>> {
public:
    using Binding=VerticalWorldSessionSaveBinding<Store>;
    using Preparation=VerticalWorldSessionPreparation<Resource,Binding>;
    using Failure=std::function<void(std::exception_ptr)>;
    static std::shared_ptr<VerticalWorldSessionSaveTeardown> create(VerticalWorldSession::Strand strand,
        std::shared_ptr<Preparation> preparation,Failure failure) {
        if(!strand.running_in_this_thread() || !preparation || !failure)throw std::invalid_argument("Incomplete session save teardown");
        return std::shared_ptr<VerticalWorldSessionSaveTeardown>(new VerticalWorldSessionSaveTeardown(std::move(strand),std::move(preparation),std::move(failure)));
    }
    // Completion is called on the world strand only after both drains succeed.
    // Failure deliberately never calls completion or releases the session.
    void start(std::function<void()> done) {
        require_strand();if(started_ || !done)throw std::logic_error("Invalid session save teardown start");
        started_=true;auto self=this->shared_from_this();
        try {
            auto binding=preparation_->binding();if(!binding)throw std::logic_error("Missing prepared save binding");
            binding->stop_and_drain([self,done=std::move(done)]() mutable {
                try {self->preparation_->release_binding([self,done=std::move(done)](std::exception_ptr error) mutable {
                    if(error){self->notify(error);return;}
                    try {done();}catch(...) {self->notify(std::current_exception());}
                });}catch(...) {self->notify(std::current_exception());}
            });
        } catch(...) {notify(std::current_exception());}
    }
private:
    VerticalWorldSessionSaveTeardown(VerticalWorldSession::Strand strand,std::shared_ptr<Preparation> preparation,Failure failure)
        :strand_(std::move(strand)),preparation_(std::move(preparation)),failure_(std::move(failure)){}
    void require_strand() const {if(!strand_.running_in_this_thread())throw std::logic_error("Session save teardown outside world strand");}
    void notify(std::exception_ptr error) noexcept {try{failure_(error);}catch(...) {}}
    VerticalWorldSession::Strand strand_;std::shared_ptr<Preparation> preparation_;Failure failure_;bool started_=false;
};
using VerticalWorldSessionSqlSaveTeardown=VerticalWorldSessionSaveTeardown<VerticalWorldSessionSqlSave>;
}
