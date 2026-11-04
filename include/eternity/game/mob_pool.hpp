#pragma once
#include "eternity/game/mobs.hpp"
#include <memory>

namespace eternity::game {
class MobAppliedSkillsRuntime;
class MobPoolServices {
public:
    virtual ~MobPoolServices()=default;
    virtual std::unique_ptr<MobRuntime> construct()=0;
    virtual std::uint32_t unix_time()=0;
    virtual MobDamagePool& damage_pool()=0;
    virtual void release_compiled_ai(MobRuntime&)=0;
    virtual void release_stack_ai(MobRuntime&)=0;
    virtual void close_lua(void*)=0;
};
// Serialized object-manager owner. All members must leave their zones before
// recycling/destruction. Raw zone/cell values may survive bulk release.
// The world counter and services outlive this pool.
// CreateMob 0040a5e0 / FreeMob 0040a630, SSyncHeap<CMob>::Alloc 0040ab80.
class MobPool {
public:
    MobPool(std::uint32_t& counter,MobPoolServices& services):counter_(counter),services_(services) {}
    // Production constructor: every fresh actor binds one retained skill owner
    // and destructor AI services. All borrowed owners outlive the entire pool.
    MobPool(std::uint32_t& counter,MobPoolServices& services,MobAppliedSkillsRuntime& skills)
        :counter_(counter),services_(services),skills_(&skills){}
    // Two-argument constructor above is the historical damage-only projection;
    // it cannot provide applied skills and does not fabricate lifetime ports.
    // Alloc 0040ab80: constructor/binding on fresh objects, LIFO reuse otherwise.
    // Does not change retained ID/random state or consume the manager counter.
    MobRuntime* allocate_raw();
    bool has_applied_skills_runtime()const noexcept{return skills_!=nullptr;}
    // Live host ownership query; false for foreign, available or raw-pending actors.
    bool is_prepared(const MobRuntime&)const noexcept;
    // Exactly one manager preparation for an outstanding raw allocation:
    // increment world ID, then sample Unix time and seed time^ID.
    // Foreign, released or already prepared objects are rejected without writes.
    void prepare_allocated(MobRuntime&);
    MobRuntime* allocate();
    void release(MobRuntime&);
    std::uint32_t construction_attempts() const noexcept {return construction_attempts_;}
    std::size_t available() const noexcept {return available_.size();}
    std::size_t size() const noexcept {return objects_.size();}
private:
    std::uint32_t& counter_;MobPoolServices& services_;
    MobAppliedSkillsRuntime* skills_=nullptr;
    std::uint32_t construction_attempts_=0;
    std::vector<std::unique_ptr<MobRuntime>> objects_;
    std::vector<MobRuntime*> available_;
    std::vector<MobRuntime*> unprepared_;
};
}
