#pragma once
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace eternity::game {
struct SkillDefinition;
// Explicit CSkill payload projection; links remain owned by the native container.
struct MobAppliedSkill {
    std::uint32_t instance_id=0, skill_id=0, level=0;
    std::uint32_t number=0, attack_tick=0, reset_tick=0;
    std::uint32_t end_tick=0, previous_tick=0, target_type=0, target_id=0;
    const SkillDefinition* definition=nullptr; // Borrowed; definition storage outlives active/recycled skills.
    std::string name;
};
class MobAppliedSkillPoolServices {
public:
    virtual ~MobAppliedSkillPoolServices()=default;
    virtual std::unique_ptr<MobAppliedSkill> construct()=0;
};
class MobAppliedSkillLogServices {
public:
    virtual ~MobAppliedSkillLogServices()=default;
    // Required owner-bound SendLogBothMob boundary, called before unlink/recycle.
    virtual void removed(const MobAppliedSkill&, std::uint8_t action) noexcept=0;
};
class MobAppliedSkills;
// One object-manager heap shared across owners. It must outlive every list.
class MobAppliedSkillPool {
public:
    explicit MobAppliedSkillPool(MobAppliedSkillPoolServices& services):services_(services) {}
    std::vector<MobAppliedSkill*> available() const;
    std::uint32_t construction_attempts() const;
    std::size_t owned_count() const;
private:
    friend class MobAppliedSkills;
    MobAppliedSkill* acquire();
    void recycle(MobAppliedSkill*);
    void discard(MobAppliedSkill*);
    MobAppliedSkillPoolServices& services_;
    mutable std::mutex mutex_;
    std::uint32_t attempts_=0;
    std::vector<std::unique_ptr<MobAppliedSkill>> objects_;
    std::vector<MobAppliedSkill*> free_;
};
struct MobAppliedSkillLogProjection {
    bool lua_ai=false; // CMob +0x284 != 0
    bool respawn_present=false; // CMob +0xc84 != 0
    bool respawn_logging=false; // stMobRespawn::bBothMob +0x1c != 0
};
class MobAppliedSkills {
public:
    explicit MobAppliedSkills(MobAppliedSkillPool& pool, MobAppliedSkillLogServices& log):pool_(pool),log_(log) {}
    ~MobAppliedSkills();
    MobAppliedSkills(const MobAppliedSkills&)=delete;
    MobAppliedSkills& operator=(const MobAppliedSkills&)=delete;
    // Ownership admission only: not AddApplySkill's effect/replacement logic.
    // Reused payload is deliberately retained, as in SSyncHeap<CSkill>::Alloc.
    MobAppliedSkill* append_from_pool();
    // CObjectMgr::CreateSkill 0040a780 followed by AddBack. The counter is the
    // object manager's shared skill counter, not a per-mob sequence. A wrapped
    // zero ID remains zero until the next creation of that recycled object.
    MobAppliedSkill* append_created(std::atomic<std::uint32_t>& counter);
    std::vector<MobAppliedSkill*> records() const;
    void clear(const MobAppliedSkillLogProjection&);
private:
    MobAppliedSkill* append(std::atomic<std::uint32_t>* counter);
    MobAppliedSkillPool& pool_;
    MobAppliedSkillLogServices& log_;
    mutable std::recursive_mutex mutex_;
    std::vector<MobAppliedSkill*> records_;
};
}
