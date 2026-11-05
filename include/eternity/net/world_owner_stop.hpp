#pragma once
#include "eternity/net/world_server.hpp"
#include <asio/any_io_executor.hpp>
#include <asio/executor_work_guard.hpp>
#include <asio/post.hpp>
#include <asio/strand.hpp>
#include <exception>
#include <functional>
#include <stdexcept>
#include <memory>
#include <mutex>
#include <optional>
#include <type_traits>
#include <utility>

namespace eternity::net {
// Coordinates the real transport and a retained session owner exposing
// registry() and shutdown(). Call only after stopping all other session
// producers and fencing unsolicited traffic. Session disconnect services keep
// their normal DB/final-save producers until their own actual drain completes.
// The caller keeps both IO contexts, DB workers, owner dependencies and this
// handle alive. Completion covers only this transport and session owner, not a
// separate WorldRoot, zones, SQL pools or externally retained reservations.
// This owns the registry's one-shot close_admission_and_wait registration;
// do not also start VerticalShutdownDrain on the same registry. After actual
// final-save submissions, other real tracker drains may use their own barrier.
// All methods except transport_post_error() run on the owner strand; callbacks
// must not throw. The transport identity must come from the same bound owner
// that supplies its factory; the identity cannot inspect that factory. The
// transport must also use the bound issuer and realm.
// Failures do not fabricate cleanup or release account fences.
template<class Owner> class WorldOwnerStop final:public std::enable_shared_from_this<WorldOwnerStop<Owner>> {
public:
    using Strand=asio::strand<asio::any_io_executor>;
    using Failure=std::function<void(std::exception_ptr)>;
    struct TransportPending {std::size_t submissions;std::size_t notifications;};
    // Optional before_transport_post is a failure-injection seam; it runs on
    // the transport executor after the real stop result has been retained.
    static std::shared_ptr<WorldOwnerStop> create(Strand strand,std::shared_ptr<WorldServer> transport,
        std::shared_ptr<Owner> owner,std::function<void()> done,Failure failure,
        std::function<void()> before_transport_post={}) {
        if(!strand.running_in_this_thread() || !transport || !owner || !done || !failure)
            throw std::invalid_argument("World owner stop requires its strand and actual owners");
        auto identity=owner->registry().owner_identity();
        if(!identity || identity!=transport->owner_identity() ||
           !owner->registry().matches_network_binding(transport->network_registry(),transport->realm()))
            throw std::invalid_argument("World transport and session owner bindings differ");
        return std::shared_ptr<WorldOwnerStop>(new WorldOwnerStop(std::move(strand),std::move(transport),std::move(owner),std::move(done),std::move(failure),std::move(before_transport_post)));
    }
    // Keep this handle before calling start. False reports a submission error;
    // it does not assert transport or session cleanup occurred.
    bool start() {
        require_strand();if(stage_!=Stage::fresh)throw std::logic_error("World owner stop already started");
        stage_=Stage::transport;auto self=this->shared_from_this();
        try {transport_->stop([self](bool ok){self->post_transport_result(ok);});return true;}
        catch(...) {stage_=Stage::failed;report(std::current_exception());return false;}
    }
    // Posting transport completion can fail before it reaches the owner strand.
    // The real result remains pending; inspect this and retry on that strand.
    std::exception_ptr transport_post_error() const {std::lock_guard lock(mutex_);return post_error_;}
    std::exception_ptr error() const {
        require_strand();if(error_)return error_;
        if(stage_==Stage::sessions && registry_)return registry_->drain_notification_error();
        return transport_post_error();
    }
    // A snapshot of actual retained transport work. Both IO contexts and
    // worker executors must remain alive until their real completion.
    TransportPending transport_pending() const {
        require_strand();return {transport_->pending_close_submissions(),transport_->pending_close_notifications()};
    }
    // Retry actual pending transport submissions/notifications, the owner-post
    // notification, session notification or owner shutdown. A failed transport
    // close remains a failure and is not converted into success.
    void retry() {
        require_strand();
        if(stage_==Stage::transport){
            auto keep_alive=this->shared_from_this();consume_transport_result();
            if(stage_!=Stage::transport)return;
            try {
                if(transport_->pending_close_submissions())transport_->retry_close_submissions();
                if(transport_->pending_close_notifications())transport_->retry_close_notifications();
            } catch(...) {report(std::current_exception());}
            return;
        }
        if(stage_==Stage::sessions && registry_){registry_->retry_drain_notification();error_=registry_->drain_notification_error();return;}
        if(stage_==Stage::owner_retry){finish_owner();return;}
        throw std::logic_error("World owner stop has nothing retryable");
    }
private:
    using Registry=std::remove_reference_t<decltype(std::declval<Owner&>().registry())>;
    enum class Stage {fresh,transport,sessions,owner_retry,failed,complete};
    WorldOwnerStop(Strand strand,std::shared_ptr<WorldServer> transport,std::shared_ptr<Owner> owner,
        std::function<void()> done,Failure failure,std::function<void()> before_transport_post)
        :strand_(std::move(strand)),guard_(asio::make_work_guard(asio::any_io_executor(strand_))),transport_(std::move(transport)),owner_(std::move(owner)),done_(std::move(done)),failure_(std::move(failure)),before_transport_post_(std::move(before_transport_post)) {}
    void require_strand() const {if(!strand_.running_in_this_thread())throw std::logic_error("World owner stop requires its strand");}
    void post_transport_result(bool ok) noexcept {
        {std::lock_guard lock(mutex_);pending_=ok;}
        try {
            auto self=this->shared_from_this();
            if(before_transport_post_)before_transport_post_(); // Optional failure-injection seam.
            asio::post(strand_,[self]{self->consume_transport_result();});
        } catch(...) {
            std::lock_guard lock(mutex_);
            // An owner-strand retry may already have consumed the real result
            // while the post was in progress; do not retain a stale error.
            if(pending_.has_value())post_error_=std::current_exception();
        }
    }
    void consume_transport_result() {
        require_strand();std::optional<bool> result;
        {std::lock_guard lock(mutex_);result=std::exchange(pending_,{});post_error_={};}
        if(!result || stage_!=Stage::transport)return;
        if(!*result) {stage_=Stage::failed;report(std::make_exception_ptr(std::runtime_error("World transport close failed")));return;}
        try {
            registry_=&owner_->registry();stage_=Stage::sessions;auto weak=this->weak_from_this();
            registry_->close_admission_and_wait(strand_,[weak]{if(auto self=weak.lock())self->finish_owner();});
            if(auto error=registry_->drain_notification_error())report(error);
        } catch(...) {stage_=Stage::failed;report(std::current_exception());}
    }
    void finish_owner() {
        require_strand();if(stage_!=Stage::sessions && stage_!=Stage::owner_retry)return;
        try {owner_->shutdown();}
        catch(...) {stage_=Stage::owner_retry;report(std::current_exception());return;}
        stage_=Stage::complete;error_={};auto done=std::move(done_);guard_.reset();done();
    }
    void report(std::exception_ptr error) {error_=error;try{failure_(error);}catch(...) {}}
    Strand strand_;asio::executor_work_guard<asio::any_io_executor> guard_;
    std::shared_ptr<WorldServer> transport_;std::shared_ptr<Owner> owner_;Registry* registry_=nullptr;
    std::function<void()> done_;Failure failure_;std::function<void()> before_transport_post_;Stage stage_=Stage::fresh;std::exception_ptr error_;
    mutable std::mutex mutex_;std::optional<bool> pending_;std::exception_ptr post_error_;
};
}
