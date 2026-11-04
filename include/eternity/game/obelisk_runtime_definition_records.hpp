#pragma once
#include "eternity/game/obelisk_runtime_config.hpp"
#include <cstddef>
namespace eternity::game {
// Required owner access resolves the actual retained 268-byte stMOBInfo record.
// Bind e.g. canonical_catalog.mutable_mob_record(id); no copied record/cache.
// Serialize catalog publication/reload and retain owner/storage through callbacks.
class ObeliskRuntimeDefinitionRecords final:public ObeliskRuntimeDefinitionOwner {
public:
    using Access=std::function<std::span<std::byte>(std::uint32_t)>;
    explicit ObeliskRuntimeDefinitionRecords(Access);
    void mark_obelisk(const MobRuntimeTemplate&) override;
private:
    Access access_;
};
}
