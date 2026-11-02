#pragma once
#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <memory>

namespace eternity::game {
// ps_dbAgent.exe SDBDispatchClient 00431a60/00431ac0. Atomics return the
// incremented count / pre-subtraction count respectively. Callbacks do not throw.
struct DbDispatchPort {
    bool attached=true,synchronous=false;
    std::function<void(std::uint32_t)> push,dispatch;
    std::function<std::uint32_t()> pop,count,increment;
    std::function<std::uint32_t(std::uint32_t)> subtract;
    std::function<void()> wake;
};
void post_db_job(std::uint32_t job,const DbDispatchPort&);
void dispatch_db_jobs(const DbDispatchPort&);

class DbJobClient;
struct DbCompletionQueue;
// Native completion-queue/worker replacement for the original Windows IOCP.
// Source control rules remain in the two functions above. Duplicate completions
// can overlap for one client even in synchronous mode; handlers serialize their
// owned state separately. Destruction/shutdown occurs outside worker callbacks;
// callers serialize shutdown calls.
class DbJobDispatcher {
public:
    explicit DbJobDispatcher(bool synchronous,unsigned int workers=0);
    ~DbJobDispatcher();
    DbJobDispatcher(const DbJobDispatcher&)=delete;
    DbJobDispatcher& operator=(const DbJobDispatcher&)=delete;
    std::shared_ptr<DbJobClient> client(std::function<void(std::uint32_t)>);
    // Producers stop first. Waits for completion tokens/active callbacks only;
    // original counter races or zero statuses may strand counted/queued work.
    void wait_idle();
    // Source shutdown posts one null completion per worker, then joins. Work
    // requeued behind those sentinels is not promised a drain. Admission closes
    // first in this native adapter; post() then reports false.
    void shutdown();
    std::exception_ptr failure() const;
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
class DbJobClient : public std::enable_shared_from_this<DbJobClient> {
public:
    ~DbJobClient();
    bool post(std::uint32_t job);
    // Reset requires external ownership coordination, as in original CUser reset.
    void reset();
    std::uint32_t pending_count() const noexcept;
    std::size_t queued_count() const;
private:
    friend class DbJobDispatcher;friend struct DbCompletionQueue;
    struct Impl;
    DbJobClient(std::weak_ptr<DbCompletionQueue>,std::function<void(std::uint32_t)>);
    void dispatch();
    std::unique_ptr<Impl> impl_;
};
}
