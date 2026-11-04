#include "eternity/game/obelisk_runtime_zone.hpp"
#include <stdexcept>

namespace eternity::game {
bool ObeliskRuntimeZoneRemoval::remove_by_id(std::uint32_t id) {return zone_.remove_by_id(id,pool_,*this);}
void ObeliskRuntimeZoneRemoval::destroy_obelisk(MobRuntime& mob,std::int32_t type) {
    // CZone::MobRemoveByID 0041e9c3–0041e9f4. Original accepts the unsafe
    // one-past index too; require real storage instead of dereferencing it.
    if(type<0 || static_cast<std::size_t>(type)>records_.size()) return;
    if(static_cast<std::size_t>(type)==records_.size()) throw std::out_of_range("One-past obelisk removal index is unsupported");
    auto& o=records_[static_cast<std::size_t>(type)];
    eternity::game::destroy_obelisk(map_,o,static_cast<std::size_t>(type),mob,o.base_country,true,kill_count_add_,world_);
}
void ObeliskRuntimeZoneRemoval::remove_obelisk_member(MobRuntime& mob,void* source) {
    for(auto& o:records_) for(auto& spawn:o.spawns) if(source==&spawn) {spawn.remove(&mob);return;}
    throw std::invalid_argument("Obelisk member source is not bound to this map");
}
void ObeliskRuntimeZoneRemoval::destroy_linked_group(std::int32_t type) {linked_.destroy_linked_group(type);}
void ObeliskRuntimeZoneRemoval::remove_linked_member(MobRuntime& mob,void* source) {linked_.remove_linked_member(mob,source);}
}
