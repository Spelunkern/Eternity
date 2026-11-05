#pragma once
#include "eternity/game/vertical_world_session_sql_save.hpp"
#include <asio/post.hpp>
#include <atomic>

namespace eternity::game {
// One asynchronous resource open for a retained session. The opener executes on
// the DB executor; all other callbacks and methods run on the world strand.
// Both executors, the registry and every resource dependency must outlive an
// accepted open. The caller retains this object and cancels/drains it before
// releasing the session, in addition to fencing all other producers and traffic.
template<class Resource> class VerticalWorldSessionResourceOpen final:public std::enable_shared_from_this<VerticalWorldSessionResourceOpen<Resource>> {
public:
    using Strand=VerticalWorldSession::Strand;
    using Open=std::function<std::shared_ptr<Resource>()>;
    using Loaded=std::function<void(VerticalWorldSession&,std::shared_ptr<Resource>)>;
    using Failure=std::function<void(VerticalWorldSession&,std::exception_ptr)>;
    static std::shared_ptr<VerticalWorldSessionResourceOpen> create(Strand strand,asio::any_io_executor database,
        VerticalWorldSession::Registry& registry,std::shared_ptr<VerticalWorldSession> session,Open open,Loaded loaded,Failure failure) {
        if(!strand.running_in_this_thread() || !session || registry.find_account(session->account_id())!=session || !open || !loaded || !failure)
            throw std::invalid_argument("Incomplete session resource open");
        return std::shared_ptr<VerticalWorldSessionResourceOpen>(new VerticalWorldSessionResourceOpen(std::move(strand),std::move(database),registry,std::move(session),std::move(open),std::move(loaded),std::move(failure)));
    }
    void start() {
        require_strand();if(started_ || cancelled_.load(std::memory_order_acquire))throw std::logic_error("Session resource open already stopped or started");
        started_=true;auto self=this->shared_from_this();
        if(!registry_.begin_database_work(session_->account_id(),*session_))throw std::logic_error("Resource open fence rejected");
        pending_=true;
        try {asio::post(database_,[self] {
            std::shared_ptr<Resource> resource;std::exception_ptr error;
            if(!self->cancelled_.load(std::memory_order_acquire)) {
                try {resource=self->open_();if(!resource)throw std::runtime_error("Session resource open returned null");}catch(...){error=std::current_exception();}
            }
            try {asio::post(self->strand_,[self,resource=std::move(resource),error]() mutable {
                if(!self->cancelled_.load(std::memory_order_acquire)) {
                    try {if(error)self->failure_(*self->session_,error);else self->loaded_(*self->session_,resource);}
                    catch(...) {self->notify(std::current_exception());}
                }
                // Release the opener's reference on a DB worker, including
                // canceled and failed binding paths, before clearing the fence.
                if(resource)self->dispose(std::move(resource));else self->finish();
            });} catch(...) {
                // No world-strand callback can safely report this failure.
                // Keep the account fenced and never signal a successful drain.
            }
        });} catch(...) {
            auto error=std::current_exception();
            try {if(!registry_.finish_database_work(session_->account_id(),*session_))throw std::logic_error("Resource open fence lost owner");pending_=false;}
            catch(...) {notify(std::current_exception());}
            std::rethrow_exception(error);
        }
    }
    void cancel_and_drain(std::function<void()> done) {
        require_strand();if(!done)throw std::invalid_argument("Missing resource drain callback");auto self=this->shared_from_this();cancelled_.store(true,std::memory_order_release);
        if(!pending_){done();return;}waiters_.push_back(std::move(done));
    }
    bool pending() const {require_strand();return pending_;}
private:
    VerticalWorldSessionResourceOpen(Strand strand,asio::any_io_executor database,VerticalWorldSession::Registry& registry,
        std::shared_ptr<VerticalWorldSession> session,Open open,Loaded loaded,Failure failure)
        :strand_(std::move(strand)),database_(std::move(database)),registry_(registry),session_(std::move(session)),
         open_(std::move(open)),loaded_(std::move(loaded)),failure_(std::move(failure)){}
    void require_strand() const {if(!strand_.running_in_this_thread())throw std::logic_error("Session resource open outside world strand");}
    void notify(std::exception_ptr error) noexcept {try{failure_(*session_,error);}catch(...) {}}
    void dispose(std::shared_ptr<Resource> resource) {
        auto self=this->shared_from_this();
        disposal_=std::move(resource);
        try {
            asio::post(database_,[self] {
                self->disposal_.reset();
                try {asio::post(self->strand_,[self]{self->finish();});}
                catch(...) { /* Keep the account fenced if world completion cannot be posted. */ }
            });
        } catch(...) {
            // The retained holder prevents a blocking destructor on this strand.
            // Failure to queue disposal deliberately leaves the fence pending.
            notify(std::current_exception());
        }
    }
    void finish() {
        try {if(!registry_.finish_database_work(session_->account_id(),*session_))throw std::logic_error("Resource open fence lost owner");}
        catch(...) {notify(std::current_exception());return;}
        pending_=false;auto waiters=std::move(waiters_);waiters_.clear();
        for(auto& waiter:waiters)try{waiter();}catch(...){notify(std::current_exception());}
    }
    Strand strand_;asio::any_io_executor database_;VerticalWorldSession::Registry& registry_;std::shared_ptr<VerticalWorldSession> session_;
    Open open_;Loaded loaded_;Failure failure_;std::shared_ptr<Resource> disposal_;std::atomic<bool> cancelled_{false};bool started_=false,pending_=false;std::vector<std::function<void()>> waiters_;
};
using VerticalWorldSessionSqlSaveOpen=VerticalWorldSessionResourceOpen<VerticalWorldSessionSqlSave>;
// The returned callable opens only the supplied configured SQL Server; it does
// not provision/reset databases. The recovery path must be in ignored runtime data.
inline VerticalWorldSessionSqlSaveOpen::Open vertical_world_session_sql_save_opener(storage::SqlServerConfig config,std::uint8_t server,
    std::filesystem::path recovery_path,std::string catalog="PS_GameData",std::shared_ptr<std::recursive_mutex> mutex={}) {
    if(recovery_path.empty() || catalog.empty())throw std::invalid_argument("Missing save recovery path or catalog");
    return [config=std::move(config),server,recovery_path=std::move(recovery_path),catalog=std::move(catalog),mutex=std::move(mutex)] {
        return VerticalWorldSessionSqlSave::open(config,server,recovery_path,catalog,mutex);
    };
}
}
