#pragma once
#include "eternity/game/obelisk_runtime.hpp"
#include "eternity/game/zone_mobs.hpp"
#include "eternity/game/mob_pool.hpp"

namespace eternity::game {
// Live forced-removal bridge, not a second zone/pool implementation. All owners
// and projected record/spawn storage must remain stable through nested removal.
// World::remove_by_id calls this bridge's remove_by_id; linked callbacks require
// their actual owner. Forced removal uses base country/reset=true, not kill credit.
class ObeliskRuntimeZoneRemoval final:public MobRemovalOwners {
public:
    ObeliskRuntimeZoneRemoval(ObeliskRuntimeMap& map,std::span<ObeliskRuntime> records,
        ZoneMobs& zone,MobPool& pool,ObeliskRuntimeWorld& world,
        const std::array<std::int32_t,2>& kill_count_add,MobRemovalOwners& linked)
        :map_(map),records_(records),zone_(zone),pool_(pool),world_(world),kill_count_add_(kill_count_add),linked_(linked) {}
    bool remove_by_id(std::uint32_t);
    void destroy_obelisk(MobRuntime&,std::int32_t type) override;
    void remove_obelisk_member(MobRuntime&,void* source) override;
    void destroy_linked_group(std::int32_t type) override;
    void remove_linked_member(MobRuntime&,void* source) override;
private:
    ObeliskRuntimeMap& map_;std::span<ObeliskRuntime> records_;
    ZoneMobs& zone_;MobPool& pool_;ObeliskRuntimeWorld& world_;
    const std::array<std::int32_t,2>& kill_count_add_;MobRemovalOwners& linked_;
};
}
