#pragma once
#include "eternity/game/obelisk_runtime_zone.hpp"

namespace eternity::game {
// Concrete ownership part of MobDeathWorld. Reward/quest/skill/transport providers
// remain separate. Context is the primary map/zone owning this dying actor.
class ObeliskRuntimeDeathOwnership {
public:
    ObeliskRuntimeDeathOwnership(ObeliskRuntimeMap& map,std::span<ObeliskRuntime> records,
        ZoneMobs& zone,ObeliskRuntimeWorld& world,const std::array<std::int32_t,2>& kill_count_add,
        ObeliskRuntimeZoneRemoval& members)
        :map_(map),records_(records),zone_(zone),world_(world),kill_count_add_(kill_count_add),members_(members) {}
    std::optional<std::int32_t> country(MobRuntime&,std::int32_t type) const;
    void destroyed(MobRuntime&,std::int32_t country,std::int32_t type);
    void remove_member(MobRuntime&,void* source);
private:
    void context(const MobRuntime&) const;
    ObeliskRuntime* record(std::int32_t type) const;
    ObeliskRuntimeMap& map_;std::span<ObeliskRuntime> records_;ZoneMobs& zone_;
    ObeliskRuntimeWorld& world_;const std::array<std::int32_t,2>& kill_count_add_;
    ObeliskRuntimeZoneRemoval& members_;
};
}
