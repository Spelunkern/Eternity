#include "eternity/game/obelisk_runtime_death.hpp"
#include <stdexcept>

namespace eternity::game {
void ObeliskRuntimeDeathOwnership::context(const MobRuntime& mob) const {
    if(mob.zone!=&zone_) throw std::invalid_argument("Dying obelisk is not attached to its owning zone");
}
ObeliskRuntime* ObeliskRuntimeDeathOwnership::record(std::int32_t type) const {
    if(type<0 || static_cast<std::size_t>(type)>records_.size() || !records_.data()) return nullptr;
    if(static_cast<std::size_t>(type)==records_.size()) throw std::out_of_range("One-past obelisk death index is unsupported");
    return &records_[static_cast<std::size_t>(type)];
}
std::optional<std::int32_t> ObeliskRuntimeDeathOwnership::country(MobRuntime& mob,std::int32_t type) const {
    context(mob);const auto* o=record(type);return o?std::optional{o->country}:std::nullopt;
}
void ObeliskRuntimeDeathOwnership::destroyed(MobRuntime& mob,std::int32_t country,std::int32_t type) {
    context(mob);auto* o=record(type);if(!o) throw std::invalid_argument("Dying obelisk has no bound ownership record");
    // CMob::Death 004910f0 calls reset=false. Its caller-side account() alone
    // sets next=UINT32_MAX/decrements current; this adapter must not do it again.
    destroy_obelisk(map_,*o,static_cast<std::size_t>(type),mob,country,false,kill_count_add_,world_);
}
void ObeliskRuntimeDeathOwnership::remove_member(MobRuntime& mob,void* source) {
    context(mob);members_.remove_obelisk_member(mob,source);
}
}
