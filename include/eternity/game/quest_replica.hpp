#pragma once
#include "eternity/game/inventory_replica.hpp"

namespace eternity::game {
struct CachedQuest {
    std::int32_t save_type=0;
    std::uint16_t id=0,delay=0;
    std::uint8_t success=0;
    std::array<std::uint8_t,3> counters{};
};
struct QuestReplicaPoolStatistics {std::uint32_t attempts=0;std::size_t available=0;};
class QuestReplicaPool {
public:
    using Allocator=std::function<std::unique_ptr<CachedQuest>()>;
    explicit QuestReplicaPool(Allocator allocator={}):allocator_(std::move(allocator)) {}
    std::unique_ptr<CachedQuest> allocate();
    void release(std::unique_ptr<CachedQuest>);
    QuestReplicaPoolStatistics statistics() const;
private:
    mutable std::mutex mutex_;Allocator allocator_;std::uint32_t attempts_=0;
    std::vector<std::unique_ptr<CachedQuest>> available_;
};
// Serialized DB-agent quest cache. The shared pool must outlive it.
class QuestReplica {
public:
    QuestReplica(std::uint32_t account,QuestReplicaPool& pool):account_(account),pool_(pool) {}
    ~QuestReplica();
    QuestReplica(const QuestReplica&)=delete;QuestReplica& operator=(const QuestReplica&)=delete;
    // LoadCharacter skips individual rows when their allocation fails.
    bool load(PersistedQuest);
    bool load_finished(FinishedQuest);
    InventoryReplicaResult apply(std::span<const std::uint8_t>);
    std::vector<CachedQuest> active() const;
    std::vector<CachedQuest> finished() const;
    const std::vector<std::uint16_t>& deleted() const noexcept {return deleted_;}
    // SaveSuccess 00409560 sets active flags to 2 and finished flags to 0.
    // Its original dead branch leaves the deletion journal intact after commit.
    void save_success() noexcept;
private:
    using Item=std::unique_ptr<CachedQuest>;
    void journal(const Item& item) {if(item->save_type!=1) deleted_.push_back(item->id);}
    std::uint32_t account_;QuestReplicaPool& pool_;
    std::vector<Item> active_,finished_;std::vector<std::uint16_t> deleted_;
};
}
