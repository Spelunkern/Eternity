#pragma once
#include "eternity/game/quests.hpp"
#include <memory>
#include <mutex>
#include <map>
#include <span>
#include <vector>

namespace eternity::game {
class QuestRuntime;
class QuestRuntimePoolServices {
public:
    virtual ~QuestRuntimePoolServices()=default;
    virtual std::unique_ptr<RestoredQuest> construct()=0;
};
// Shared serialized CQuest heap. Definitions, services and pool must outlive
// runtime owners. FreeQuest retains payload; later allocations reuse it LIFO.
class QuestRuntimePool {
public:
    explicit QuestRuntimePool(QuestRuntimePoolServices& services):services_(services) {}
    RestoredQuest* allocate();
    void release(RestoredQuest&);
    std::uint32_t construction_attempts() const noexcept {return attempts_;}
    std::span<RestoredQuest* const> available() const noexcept {return free_;}
private:
    friend class QuestRuntime;
    void claim(RestoredQuest&,QuestRuntime*);
    void unclaim(RestoredQuest&);
    void discard(RestoredQuest&);
    QuestRuntimePoolServices& services_;std::uint32_t attempts_=0;
    std::vector<std::unique_ptr<RestoredQuest>> storage_;std::vector<RestoredQuest*> free_;
    std::map<RestoredQuest*,QuestRuntime*> owners_;
};
class QuestRuntimeWorld {
public:
    virtual ~QuestRuntimeWorld()=default;
    virtual void send_database(const net::Packet&)=0;
};
// Stable per-user CQuest ownership. Active lookup/traversal shares the original
// cursor; removing its current quest backs it to the previous node (or head).
// Creation/admission and full user shutdown orchestration remain separate.
class QuestRuntime {
public:
    QuestRuntime(std::uint32_t user,QuestRuntimePool& pool):user_id_(user),pool_(pool) {}
    ~QuestRuntime();
    QuestRuntime(const QuestRuntime&)=delete;QuestRuntime& operator=(const QuestRuntime&)=delete;
    // Native owner binding: admission, removal and progress must share the
    // same account and CQuest heap throughout the serialized user lifetime.
    bool bound_to(std::uint32_t account,const QuestRuntimePool& pool) const noexcept {return user_id_==account && &pool_==&pool;}
    // QuestAdd appends under the list lock, then accounts outside it.
    void add_active_unaccounted(RestoredQuest&);
    void add_active(RestoredQuest&);
    void add_finished(RestoredQuest&);
    RestoredQuest* load_active(const RestoredQuest&);
    RestoredQuest* load_finished(const RestoredQuest&);
    RestoredQuest* first_active() noexcept;
    RestoredQuest* next_active() noexcept;
    RestoredQuest* find_active(std::uint16_t) noexcept; // QuestFind 0047e4b0.
    RestoredQuest* active_cursor() const noexcept {return cursor_;}
    std::span<RestoredQuest* const> active() const noexcept {return active_;}
    std::span<RestoredQuest* const> finished() const noexcept {return finished_;}
    // Finished-list entry traversal and completed-quest insertion share this
    // distinct recursive lock. Other runtime operations remain world-serialized.
    std::recursive_mutex& finished_mutex() const noexcept {return finished_mutex_;}
    std::int32_t& mob_kill_quests() noexcept {return mob_kill_;}
    std::int32_t& player_kill_quests() noexcept {return player_kill_;}
    // CUser::QuestRemove 0047e150. Required DB delivery follows finished-list
    // insertion or actual heap release. Recycled payload is still inspectable.
    void remove(RestoredQuest&,std::uint8_t success,QuestRuntimeWorld&);
private:
    std::uint32_t user_id_;QuestRuntimePool& pool_;
    std::vector<RestoredQuest*> active_,finished_;RestoredQuest* cursor_=nullptr;
    mutable std::recursive_mutex finished_mutex_;
    std::int32_t mob_kill_=0,player_kill_=0;
};
}
