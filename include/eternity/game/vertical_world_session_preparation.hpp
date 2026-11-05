#pragma once
#include "eternity/game/vertical_world_session_resource_open.hpp"

namespace eternity::game {
// One retained preparation for VerticalWorldSessionServices::prepare. Open runs
// on the DB executor; binding, completion and cancellation run on the world
// strand. The binder must not start jobs or publish aliases before readiness.
// The owning service retains this object through cancellation/drain and
// retains the resulting binding through the full save lifecycle.
template<class Resource,class Binding> class VerticalWorldSessionPreparation final:public std::enable_shared_from_this<VerticalWorldSessionPreparation<Resource,Binding>> {
public:
    using Opener=VerticalWorldSessionResourceOpen<Resource>;
    using Open=typename Opener::Open;
    using Bind=std::function<std::shared_ptr<Binding>(VerticalWorldSession&,std::shared_ptr<Resource>)>;
    using AfterBind=std::function<void(VerticalWorldSession&,Binding&)>;
    using Completion=std::function<void(std::exception_ptr)>;
    static std::shared_ptr<VerticalWorldSessionPreparation> create(VerticalWorldSession::Strand strand,asio::any_io_executor database,
        VerticalWorldSession::Registry& registry,std::shared_ptr<VerticalWorldSession> session,Open open,Bind bind,AfterBind after_bind={}) {
        if(!strand.running_in_this_thread() || !session || !open || !bind)throw std::invalid_argument("Incomplete session preparation");
        auto result=std::shared_ptr<VerticalWorldSessionPreparation>(new VerticalWorldSessionPreparation(strand,database,registry,session,std::move(bind),std::move(after_bind)));
        std::weak_ptr<VerticalWorldSessionPreparation> weak=result;
        result->opener_=Opener::create(std::move(strand),std::move(database),registry,std::move(session),std::move(open),
            [weak](VerticalWorldSession& owner,std::shared_ptr<Resource> resource){if(auto self=weak.lock())self->opened(owner,std::move(resource));},
            [weak](VerticalWorldSession&,std::exception_ptr error){if(auto self=weak.lock())self->failed(error);});
        return result;
    }
    void start(Completion done) {
        require_strand();if(started_ || !done)throw std::logic_error("Invalid session preparation start");
        auto self=this->shared_from_this();started_=true;completion_=std::move(done);
        try {opener_->start();} catch(...) {failed(std::current_exception());}
    }
    bool matches(const VerticalWorldSession::Strand& strand,const VerticalWorldSession::Registry& registry,const VerticalWorldSession& session) const noexcept {
        return strand_==strand && &registry_==&registry && session_.get()==&session;
    }
    bool uses_strand(const VerticalWorldSession::Strand& strand) const noexcept {return strand_==strand;}
    bool fresh() const {require_strand();return !started_ && !cancelled_ && !completed_ && !releasing_;}
    // A bound but not yet published preparation has no accepted save jobs.
    // Cancellation drains the opener and retires its binding on a DB worker.
    // Cleanup failure is reported without invoking successful completion.
    void cancel(std::function<void()> done,Completion failure) {
        require_strand();if(!started_ || cancelled_ || !done || !failure)throw std::logic_error("Invalid preparation cancellation");
        // Failure cleanup can already be disposing a rejected binding while
        // the session still awaits preparation. Join that disposal rather
        // than reporting cancellation before its account fence drains.
        if(completed_) {
            if(published_ || !releasing_)throw std::logic_error("Invalid preparation cancellation");
            cancelled_=true;failure_cleanup_cancel_=std::move(done);failure_cleanup_failure_=std::move(failure);return;
        }
        cancelled_=true;completion_={};auto self=this->shared_from_this();
        opener_->cancel_and_drain([self,done=std::move(done),failure=std::move(failure)]() mutable {
            if(!self->binding_){done();return;}
            try {self->schedule_release([done=std::move(done),failure](std::exception_ptr error) mutable {
                if(error){try{failure(error);}catch(...){}return;}done();
            });} catch(...) {try{failure(std::current_exception());}catch(...) {}}
        });
    }
    // Do not expose a binding until preparation and its opener fence complete.
    std::shared_ptr<Binding> binding() const {require_strand();return published_ && !cancelled_ && !releasing_ ? binding_ : std::shared_ptr<Binding>{};}
    // Invoke after all binding jobs and external producers have stopped and
    // drained, and after other binding/store aliases have been released. The
    // extra strand turn lets the DB tracker's delivery callback return before
    // disposal begins. Only this owner's final binding reference is covered.
    void release_binding(Completion done) {
        require_strand();if(!completed_ || cancelled_ || releasing_ || !binding_ || !done)throw std::logic_error("Invalid preparation binding release");
        schedule_release(std::move(done));
    }
private:
    void schedule_release(Completion done) {
        if(releasing_)throw std::logic_error("Session binding release already started");
        releasing_=true;auto self=this->shared_from_this();
        try {asio::post(strand_,[self,done=std::move(done)]() mutable {self->begin_release(std::move(done));});}
        catch(...) {releasing_=false;throw;}
    }
    VerticalWorldSessionPreparation(VerticalWorldSession::Strand strand,asio::any_io_executor database,
        VerticalWorldSession::Registry& registry,std::shared_ptr<VerticalWorldSession> session,Bind bind,AfterBind after_bind)
        :strand_(std::move(strand)),database_(std::move(database)),registry_(registry),session_(std::move(session)),bind_(std::move(bind)),after_bind_(std::move(after_bind)){}
    void begin_release(Completion done) {
        if(binding_.use_count()!=1) {releasing_=false;done(std::make_exception_ptr(std::logic_error("Session binding still has external owners")));return;}
        std::shared_ptr<Completion> completion;
        try {completion=std::make_shared<Completion>(done);if(!registry_.begin_database_work(session_->account_id(),*session_))throw std::logic_error("Binding disposal fence rejected");}
        catch(...) {releasing_=false;done(std::current_exception());return;}
        disposal_=std::move(binding_);auto self=this->shared_from_this();
        try {asio::post(database_,[self,completion] {
            self->disposal_.reset();
            try {asio::post(self->strand_,[self,completion] {
                try {if(!self->registry_.finish_database_work(self->session_->account_id(),*self->session_))throw std::logic_error("Binding disposal fence lost owner");}
                catch(...) {(*completion)(std::current_exception());return;}
                (*completion)({});
            });} catch(...) { /* Retain the fence if world completion cannot be posted. */ }
        });} catch(...) {
            // Keep disposal_ and the account fence if the worker rejects it.
            (*completion)(std::current_exception());
        }
    }
    void require_strand() const {if(!strand_.running_in_this_thread())throw std::logic_error("Session preparation outside world strand");}
    void opened(VerticalWorldSession& owner,std::shared_ptr<Resource> resource) {
        if(cancelled_)return;
        try {
            binding_=bind_(owner,std::move(resource));if(!binding_)throw std::runtime_error("Session preparation binding returned null");
            if constexpr(requires(const Binding& binding,const VerticalWorldSession::Strand& strand,const VerticalWorldSession::Registry& registry,const VerticalWorldSession& session){binding.matches(strand,registry,session);}) {
                if(!binding_->matches(strand_,registry_,owner))throw std::logic_error("Session preparation binding has foreign owner");
            }
            if(cancelled_)return;
            if(after_bind_)after_bind_(owner,*binding_);
            if(cancelled_)return;
            finish_after_drain({});
        }
        catch(...) {failed(std::current_exception());}
    }
    void failed(std::exception_ptr error) {if(cancelled_ || completed_)return;finish_after_drain(error);}
    void finish_after_drain(std::exception_ptr error) {
        auto self=this->shared_from_this();opener_->cancel_and_drain([self,error] {
            if(self->cancelled_ || self->completed_)return;
            self->completed_=true;auto completion=std::move(self->completion_);
            if(error && self->binding_) {
                try {self->schedule_release([self,completion,error](std::exception_ptr release_error) mutable {
                    if(self->cancelled_) {
                        auto done=std::move(self->failure_cleanup_cancel_);auto failure=std::move(self->failure_cleanup_failure_);
                        if(!done || !failure)throw std::logic_error("Failure cleanup lost cancellation waiter");
                        if(release_error){try{failure(release_error);}catch(...){}return;}done();return;
                    }
                    completion(release_error?release_error:error);
                });} catch(...) {completion(std::current_exception());}
            } else {if(!error)self->published_=true;completion(error);}
        });
    }
    VerticalWorldSession::Strand strand_;asio::any_io_executor database_;VerticalWorldSession::Registry& registry_;std::shared_ptr<VerticalWorldSession> session_;
    Bind bind_;AfterBind after_bind_;std::shared_ptr<Opener> opener_;std::shared_ptr<Binding> binding_,disposal_;Completion completion_;
    std::function<void()> failure_cleanup_cancel_;Completion failure_cleanup_failure_;
    bool started_=false,cancelled_=false,completed_=false,published_=false,releasing_=false;
};
using VerticalWorldSessionSqlPreparation=VerticalWorldSessionPreparation<VerticalWorldSessionSqlSave,VerticalWorldSessionSqlSave::Binding>;
// Factory for the live SQL path. Every lifecycle port, other-reply dispatcher,
// failure handler and router is supplied by the real world service.
inline std::shared_ptr<VerticalWorldSessionSqlPreparation> make_vertical_world_session_sql_preparation(
    VerticalWorldSession::Strand strand,asio::any_io_executor database,VerticalWorldSession::Registry& registry,
    std::shared_ptr<VerticalWorldSession> session,DbJobDispatcher& dispatcher,storage::SqlServerConfig config,std::uint8_t server,
    std::filesystem::path recovery_path,std::string catalog,UserLifecycleState state,std::string username,
    CharacterSaveLifecyclePorts ports,std::shared_ptr<VerticalWorldSessionAckRouter> router,
    VerticalWorldSessionSqlSave::Binding::Other other,VerticalWorldSessionSqlSave::Binding::Failure failure,
    std::shared_ptr<std::recursive_mutex> mutex={},VerticalWorldSessionSqlPreparation::AfterBind after_bind={}) {
    auto open=vertical_world_session_sql_save_opener(std::move(config),server,std::move(recovery_path),std::move(catalog),std::move(mutex));
    return VerticalWorldSessionSqlPreparation::create(strand,std::move(database),registry,std::move(session),std::move(open),
        [strand,&registry,&dispatcher,state,username=std::move(username),ports=std::move(ports),router=std::move(router),other=std::move(other),failure=std::move(failure)]
        (VerticalWorldSession& owner,std::shared_ptr<VerticalWorldSessionSqlSave> resource) {
            return resource->bind(strand,registry,owner.shared_from_this(),dispatcher,state,username,ports,router,other,failure);
        },std::move(after_bind));
}
// The world service creates these after resource open, using the resource's
// cache mutex. Both owners must remain discoverable by the service until drain.
template<class Database> struct VerticalPreparedSocialOwners {
    std::shared_ptr<VerticalSocialSession<VerticalWorldSession,Database>> social;
    std::shared_ptr<VerticalSocialPacketJobs<VerticalWorldSession,Database>> packets;
};

template<class Resource,class Store,class Database> auto make_vertical_world_session_social_preparation(
    VerticalWorldSession::Strand strand,asio::any_io_executor database,VerticalWorldSession::Registry& registry,
    std::shared_ptr<VerticalWorldSession> session,
    typename VerticalWorldSessionResourceOpen<Resource>::Open open,
    std::function<VerticalPreparedSocialOwners<Database>(VerticalWorldSession&,std::shared_ptr<Resource>)> make_social,
    std::function<std::shared_ptr<VerticalWorldSessionSaveBinding<Store>>(VerticalWorldSession&,std::shared_ptr<Resource>,
        VerticalPreparedSocialOwners<Database>)> bind,
    typename VerticalWorldSessionPreparation<Resource,VerticalWorldSessionSaveBinding<Store>>::AfterBind after_bind={}) {
    using Preparation=VerticalWorldSessionPreparation<Resource,VerticalWorldSessionSaveBinding<Store>>;
    if(!make_social || !bind)throw std::invalid_argument("Missing social preparation services");
    return Preparation::create(strand,std::move(database),registry,std::move(session),std::move(open),
        [make_social=std::move(make_social),bind=std::move(bind)](VerticalWorldSession& owner,std::shared_ptr<Resource> resource) {
            auto social=make_social(owner,resource);
            if(!social.social || !social.packets || !social.social->owns_session(owner) ||
               !social.packets->owns_session(owner) || !social.packets->owns_social(*social.social))
                throw std::invalid_argument("Mismatched prepared social owners");
            return bind(owner,std::move(resource),std::move(social));
        },std::move(after_bind));
}

template<class Database> auto make_vertical_world_session_sql_social_preparation(
    VerticalWorldSession::Strand strand,asio::any_io_executor database,VerticalWorldSession::Registry& registry,
    std::shared_ptr<VerticalWorldSession> session,DbJobDispatcher& dispatcher,storage::SqlServerConfig config,std::uint8_t server,
    std::filesystem::path recovery_path,std::string catalog,UserLifecycleState state,std::string username,
    CharacterSaveLifecyclePorts ports,std::shared_ptr<VerticalWorldSessionAckRouter> router,
    VerticalWorldSessionSqlSave::Binding::Other other,VerticalWorldSessionSqlSave::Binding::Failure failure,
    std::shared_ptr<std::recursive_mutex> account_mutex,
    std::function<VerticalPreparedSocialOwners<Database>(VerticalWorldSession&,std::shared_ptr<VerticalWorldSessionSqlSave>)> make_social,
    VerticalWorldSessionSqlPreparation::AfterBind after_bind={}) {
    if(!account_mutex)throw std::invalid_argument("Missing retained social account mutex");
    auto open=vertical_world_session_sql_save_opener(std::move(config),server,std::move(recovery_path),std::move(catalog),std::move(account_mutex));
    return make_vertical_world_session_social_preparation<VerticalWorldSessionSqlSave,CharacterSaveStore,Database>(
        strand,std::move(database),registry,std::move(session),std::move(open),std::move(make_social),
        [strand,&registry,&dispatcher,state,username=std::move(username),ports=std::move(ports),router=std::move(router),other=std::move(other),failure=std::move(failure)]
        (VerticalWorldSession& owner,std::shared_ptr<VerticalWorldSessionSqlSave> resource,VerticalPreparedSocialOwners<Database> social) {
            return resource->bind_social(strand,registry,owner.shared_from_this(),dispatcher,state,username,ports,router,other,failure,
                std::move(social.social),std::move(social.packets));
        },std::move(after_bind));
}

}
