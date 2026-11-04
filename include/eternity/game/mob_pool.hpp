#pragma once
#include "eternity/game/mobs.hpp"
#include <memory>

namespace eternity::game {
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
    MobRuntime* allocate();
    void release(MobRuntime&);
    std::uint32_t construction_attempts() const noexcept {return construction_attempts_;}
    std::size_t available() const noexcept {return available_.size();}
    std::size_t size() const noexcept {return objects_.size();}
private:
    std::uint32_t& counter_;MobPoolServices& services_;
    std::uint32_t construction_attempts_=0;
    std::vector<std::unique_ptr<MobRuntime>> objects_;
    std::vector<MobRuntime*> available_;
};
}
