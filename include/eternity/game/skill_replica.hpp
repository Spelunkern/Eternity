#pragma once
#include "eternity/game/inventory_replica.hpp"

namespace eternity::game {
struct CachedSkill {
    std::uint16_t id=0;
    std::uint8_t level=0,number=0;
    std::uint32_t delay=0; // Actual DWORD at ps_dbAgent CSkill+c, despite older WORD PDB names.
};
struct SkillReplicaPoolStatistics {std::uint32_t attempts=0;std::size_t available=0,stranded=0;};
class SkillReplicaPool {
public:
    using Allocator=std::function<std::unique_ptr<CachedSkill>()>;
    explicit SkillReplicaPool(Allocator allocator={}):allocator_(std::move(allocator)) {}
    std::unique_ptr<CachedSkill> allocate();
    void release(std::unique_ptr<CachedSkill>);
    void strand(std::unique_ptr<CachedSkill>); // Original 0902 unlinks without recycling.
    SkillReplicaPoolStatistics statistics() const;
private:
    mutable std::mutex mutex_;Allocator allocator_;std::uint32_t attempts_=0;
    std::vector<std::unique_ptr<CachedSkill>> available_,stranded_;
};
// Serialized learned/applied skill cache. Pool and shared core skill points must
// outlive it. A false load requires discarding the partially loaded character.
class SkillReplica {
public:
    SkillReplica(std::uint32_t account,std::uint16_t& points,SkillReplicaPool& pool):account_(account),points_(points),pool_(pool) {}
    ~SkillReplica();
    SkillReplica(const SkillReplica&)=delete;SkillReplica& operator=(const SkillReplica&)=delete;
    bool load(std::span<const PlayerSkill>,std::span<const PersistedEffect>);
    InventoryReplicaResult apply(std::span<const std::uint8_t>);
    std::vector<CachedSkill> learned() const;
    std::vector<CachedSkill> effects() const;
    std::uint16_t saved_count() const noexcept {return saved_count_;}
    bool reset_pending() const noexcept {return reset_;}
    // Successful non-event SaveResetSkill job (00404c40), not request dispatch.
    // Returns 050a for the owning connection to send when present.
    net::Packet finish_reset(std::uint16_t points);
    void save_success() noexcept {saved_count_=count_;reset_=false;}
private:
    using Item=std::unique_ptr<CachedSkill>;
    std::uint32_t account_;std::uint16_t& points_;SkillReplicaPool& pool_;
    std::array<Item,256> learned_{};std::vector<Item> effects_;
    std::uint16_t count_=0,saved_count_=0;bool reset_=false;
};
}
