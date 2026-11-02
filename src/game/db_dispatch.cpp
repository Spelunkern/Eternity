#include "eternity/game/db_dispatch.hpp"
#include <algorithm>
#include <atomic>
#include <bit>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

namespace eternity::game {
namespace {
bool positive(std::uint32_t count) {return std::bit_cast<std::int32_t>(count)>0;}
void validate(const DbDispatchPort& port) {
    if(!port.push || !port.dispatch || !port.pop || !port.count || !port.increment || !port.subtract || !port.wake) throw std::invalid_argument("Incomplete DB dispatch ports");
}
}
void post_db_job(std::uint32_t job,const DbDispatchPort& port) {
    if(!port.attached) return;
    validate(port);port.push(job);
    if(port.synchronous && port.increment()!=1) return;
    port.wake();
}
void dispatch_db_jobs(const DbDispatchPort& port) {
    if(!port.attached) return;
    validate(port);
    if(!port.synchronous) {if(const auto job=port.pop()) port.dispatch(job);return;}
    auto remaining=port.count();if(!remaining) return;
    std::uint32_t done=0;
    do {--remaining;const auto job=port.pop();if(!job) break;port.dispatch(job);++done;}while(remaining);
    if(positive(done) && positive(port.count()) && positive(port.subtract(done))) port.wake();
}
struct DbCompletionQueue {
    const bool synchronous;
    mutable std::mutex mutex;
    std::condition_variable available,idle;
    std::deque<std::shared_ptr<DbJobClient>> completions;
    std::size_t active=0;
    bool accepting=true;
    std::exception_ptr failure;
    explicit DbCompletionQueue(bool sync):synchronous(sync) {}
    void wake(std::shared_ptr<DbJobClient> client) {
        {std::lock_guard lock(mutex);completions.push_back(std::move(client));}available.notify_one();
    }
    void run() {
        for(;;) {
            std::shared_ptr<DbJobClient> client;
            {std::unique_lock lock(mutex);available.wait(lock,[&](){return !completions.empty();});client=std::move(completions.front());completions.pop_front();if(!client) return;++active;}
            try {client->dispatch();}catch(...) {std::lock_guard lock(mutex);if(!failure) failure=std::current_exception();}
            {std::lock_guard lock(mutex);--active;if(!active && completions.empty()) idle.notify_all();}
        }
    }
};
struct DbJobClient::Impl {
    std::weak_ptr<DbCompletionQueue> dispatcher;
    std::function<void(std::uint32_t)> handler;
    mutable std::mutex mutex;
    std::deque<std::uint32_t> jobs;
    std::atomic<std::uint32_t> count{0};
    Impl(std::weak_ptr<DbCompletionQueue> queue,std::function<void(std::uint32_t)> callback):dispatcher(std::move(queue)),handler(std::move(callback)) {}
    DbDispatchPort port(const std::shared_ptr<DbCompletionQueue>& queue,DbJobClient& owner) {
        DbDispatchPort result;result.synchronous=queue->synchronous;
        result.push=[this](std::uint32_t job){try{std::lock_guard lock(mutex);jobs.push_back(job);}catch(const std::bad_alloc&){/* Original node allocation failure silently loses this status. */}};
        result.pop=[this](){std::lock_guard lock(mutex);if(jobs.empty()) return std::uint32_t{0};const auto job=jobs.front();jobs.pop_front();return job;};
        result.count=[this](){return count.load();};result.increment=[this](){return count.fetch_add(1)+1;};result.subtract=[this](std::uint32_t done){return count.fetch_sub(done);};result.dispatch=handler;
        result.wake=[queue,&owner](){queue->wake(owner.shared_from_this());};return result;
    }
};
struct DbJobDispatcher::Impl {
    std::shared_ptr<DbCompletionQueue> queue;
    std::vector<std::thread> workers;
    bool joined=false;
    Impl(bool sync,unsigned int count):queue(std::make_shared<DbCompletionQueue>(sync)) {
        if(!count) count=std::max(1U,std::thread::hardware_concurrency())*4U;
        count=std::min(count,256U);
        try {for(unsigned int i=0;i<count;++i) workers.emplace_back([q=queue](){q->run();});}
        catch(...) {for(std::size_t i=0;i<workers.size();++i) queue->wake(nullptr);for(auto& worker:workers) worker.join();throw;}
    }
};
DbJobDispatcher::DbJobDispatcher(bool synchronous,unsigned int workers):impl_(std::make_unique<Impl>(synchronous,workers)) {}
DbJobDispatcher::~DbJobDispatcher() {shutdown();}
std::shared_ptr<DbJobClient> DbJobDispatcher::client(std::function<void(std::uint32_t)> handler) {
    if(!handler) throw std::invalid_argument("Missing DB dispatch handler");
    std::lock_guard lock(impl_->queue->mutex);if(!impl_->queue->accepting) throw std::logic_error("DB dispatcher has shut down");
    return std::shared_ptr<DbJobClient>(new DbJobClient(impl_->queue,std::move(handler)));
}
void DbJobDispatcher::wait_idle() {std::unique_lock lock(impl_->queue->mutex);impl_->queue->idle.wait(lock,[&](){return !impl_->queue->active && impl_->queue->completions.empty();});}
void DbJobDispatcher::shutdown() {
    if(impl_->joined) return;
    {std::lock_guard lock(impl_->queue->mutex);impl_->queue->accepting=false;}
    for(std::size_t i=0;i<impl_->workers.size();++i) impl_->queue->wake(nullptr);
    for(auto& worker:impl_->workers) worker.join();
    {std::lock_guard lock(impl_->queue->mutex);impl_->queue->completions.clear();impl_->queue->idle.notify_all();}impl_->joined=true;
}
std::exception_ptr DbJobDispatcher::failure() const {std::lock_guard lock(impl_->queue->mutex);return impl_->queue->failure;}
DbJobClient::DbJobClient(std::weak_ptr<DbCompletionQueue> dispatcher,std::function<void(std::uint32_t)> handler):impl_(std::make_unique<Impl>(std::move(dispatcher),std::move(handler))) {}
DbJobClient::~DbJobClient()=default;
bool DbJobClient::post(std::uint32_t job) {
    const auto queue=impl_->dispatcher.lock();if(!queue) return false;
    // Check admission without holding the completion mutex during source wake.
    // An in-flight post may finish behind shutdown's sentinels (no drain).
    {std::lock_guard lock(queue->mutex);if(!queue->accepting) return false;}
    post_db_job(job,impl_->port(queue,*this));return true;
}
void DbJobClient::dispatch() {if(const auto queue=impl_->dispatcher.lock()) dispatch_db_jobs(impl_->port(queue,*this));}
void DbJobClient::reset() {{std::lock_guard lock(impl_->mutex);impl_->jobs.clear();}impl_->count.exchange(0);}
std::uint32_t DbJobClient::pending_count() const noexcept {return impl_->count.load();}
std::size_t DbJobClient::queued_count() const {std::lock_guard lock(impl_->mutex);return impl_->jobs.size();}
}
