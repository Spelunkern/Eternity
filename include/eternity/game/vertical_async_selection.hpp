#pragma once
#include "eternity/game/vertical_entry.hpp"
#include <asio/any_io_executor.hpp>
#include <asio/post.hpp>
#include <asio/strand.hpp>
#include <atomic>

namespace eternity::game {
// One selection, coordinated on the world strand. The loader runs on the DB
// executor and may use vertical_persistence_select; it must own or retain its
// SQL connection and pools through completion and through any returned replica.
// The owner retains this object until completion, cancels on disconnect, and
// keeps both executors running until every accepted loader has returned.
class VerticalAsyncSelection final:public std::enable_shared_from_this<VerticalAsyncSelection> {
public:
    using Strand=asio::strand<asio::any_io_executor>;
    using Load=std::function<std::optional<VerticalPersistenceSelection>(std::uint32_t,std::uint32_t)>;
    using Loaded=std::function<void(VerticalPersistenceSelection)>;
    VerticalAsyncSelection(Strand world,asio::any_io_executor database,CharacterSelectionState state,CharacterRoster roster,
        Load load,Loaded loaded,std::function<void()> unavailable,std::function<void(std::uint32_t)> close,
        std::function<void(std::exception_ptr)> failure)
        :world_(std::move(world)),database_(std::move(database)),state_(state),roster_(std::move(roster)),load_(std::move(load)),
         loaded_(std::move(loaded)),unavailable_(std::move(unavailable)),close_(std::move(close)),failure_(std::move(failure)) {
        if(!load_ || !loaded_ || !unavailable_ || !close_ || !failure_)throw std::invalid_argument("Incomplete asynchronous selection services");
    }
    CharacterSelectionResult receive(std::span<const std::uint8_t> packet) {
        require_world();if(cancelled_.load(std::memory_order_acquire))return CharacterSelectionResult::ignored;
        CharacterSelectionPorts gate;
        gate.close=close_;
        gate.send_db=[this](net::Packet request) {
            auto id=std::uint32_t{};for(unsigned i=0;i<4;++i)id|=std::uint32_t(request[6+i])<<(8*i);
            if(!vertical_entry_roster_allows(roster_,id)){unavailable_();return;}
            auto self=shared_from_this();const auto user=state_.user_id;
            if(begin_fence_ && !begin_fence_())throw std::logic_error("Selection DB fence rejected");
            pending_=true;
            try {asio::post(database_,[self,user,id] {
                std::optional<VerticalPersistenceSelection> result;std::exception_ptr error;
                if(!self->cancelled_.load(std::memory_order_acquire)) {
                    try {
                        result=self->load_(user,id);
                        if(result) {
                            if(!result->replica || result->snapshot.core.user_id!=user || result->snapshot.core.character.id!=id)
                                throw std::logic_error("Selection loader returned foreign snapshot");
                            const auto core=result->replica->core();
                            if(core.user_id!=user || core.character.id!=id)throw std::logic_error("Selection loader returned foreign replica");
                        }
                    } catch(...) {error=std::current_exception();result.reset();}
                }
                try {asio::post(self->world_,[self,result=std::move(result),error]() mutable {
                    if(!self->cancelled_.load(std::memory_order_acquire)) {
                        try {if(error)self->failure_(error);else if(result)self->loaded_(std::move(*result));else self->unavailable_();}
                        catch(...) {try{self->failure_(std::current_exception());}catch(...) {}}
                    }
                    // The worker result may own pool-backed state. Release it
                    // before notifying that cancelled work has fully drained.
                    result.reset();
                    if(self->finish_fence_) {
                        try {if(!self->finish_fence_())throw std::logic_error("Selection DB fence lost owner");}
                        catch(...) {try{self->failure_(std::current_exception());}catch(...) {}return;}
                    }
                    self->pending_=false;
                    auto waiters=std::move(self->drain_waiters_);self->drain_waiters_.clear();
                    for(auto& waiter:waiters)try{waiter();}catch(...){try{self->failure_(std::current_exception());}catch(...) {}}
                });} catch(...) {
                    // A world-strand completion cannot be reported safely from
                    // this worker. Keep the account fence and pending state;
                    // never publish a successful drain or let an exception
                    // escape the executor. The undelivered result is destroyed
                    // here on the DB worker.
                }
            });} catch(...) {
                // No DB handler was accepted. Undo the fence on this strand;
                // a failed fence completion still blocks release and drain.
                auto error=std::current_exception();
                if(finish_fence_) {
                    try {if(!finish_fence_())throw std::logic_error("Selection DB fence lost owner");}
                    catch(...) {try{failure_(std::current_exception());}catch(...) {}std::rethrow_exception(error);}
                }
                pending_=false;
                std::rethrow_exception(error);
            }
        };
        return request_character_selection(state_,packet,gate);
    }
    void cancel(){require_world();cancelled_.store(true,std::memory_order_release);}
    // Bind account tracking before the first request. A failed completion
    // deliberately leaves the job pending and never signals a successful drain.
    void set_fence(std::function<bool()> begin,std::function<bool()> finish) {
        require_world();if(!state_.enabled || pending_ || !begin || !finish || begin_fence_)throw std::logic_error("Invalid selection DB fence");
        begin_fence_=std::move(begin);finish_fence_=std::move(finish);
    }
    // Completion runs on the world strand after the DB task and its result
    // delivery/discard. It does not cover other DB producers or SQL effects.
    void when_drained(std::function<void()> done) {
        require_world();if(!done || (!cancelled_.load(std::memory_order_acquire) && state_.enabled))throw std::logic_error("Selection drain requires stopped producer");
        if(!pending_){done();return;}drain_waiters_.push_back(std::move(done));
    }
    bool pending() const {require_world();return pending_;}
private:
    void require_world() const {if(!world_.running_in_this_thread())throw std::logic_error("Selection outside world strand");}
    Strand world_;asio::any_io_executor database_;CharacterSelectionState state_;CharacterRoster roster_;Load load_;Loaded loaded_;
    std::function<void()> unavailable_;std::function<void(std::uint32_t)> close_;std::function<void(std::exception_ptr)> failure_;std::atomic<bool> cancelled_{false};bool pending_=false;
    std::function<bool()> begin_fence_,finish_fence_;
    std::vector<std::function<void()>> drain_waiters_;
};
}
