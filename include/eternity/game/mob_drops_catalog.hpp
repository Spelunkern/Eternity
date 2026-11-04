#pragma once
#include "eternity/game/mob_drops.hpp"

namespace eternity::game {
// Explicit post-load stItemInfo projection. Table coordinates preserve original
// type/type-ID scan order independently of the definition's validity fields.
struct MobDropCatalogRecord {
    std::uint8_t table_type=0,table_id=0;
    MobDropCatalogEntry* entry=nullptr;
    std::uint16_t grade=0;
    std::uint8_t server=0;
};
// ps_game.exe CItemDrop::Init 004bde50 / InitLimit 004be3e0. Borrowed entries
// must outlive the catalog; initialize only while its executor is stopped.
// Last record for a table coordinate wins. ItemCreate and sequence configuration
// are separate owners and are deliberately retained by this operation.
void initialize_mob_drop_catalog(MobDropCatalog&,std::span<const MobDropCatalogRecord>);
}

namespace eternity::game {
// Owned stable projections for the original usp_Read_Items_R result. Quest-start
// flags are initialized later by the item/quest owner, as in the original startup.
struct MobDropCatalogDefinitions {
    std::vector<MobDropCatalogEntry> entries;
    std::vector<MobDropCatalogRecord> records;
    MobDropCatalogDefinitions()=default;
    MobDropCatalogDefinitions(const MobDropCatalogDefinitions&)=delete;
    MobDropCatalogDefinitions& operator=(const MobDropCatalogDefinitions&)=delete;
    MobDropCatalogDefinitions(MobDropCatalogDefinitions&&)=default;
    MobDropCatalogDefinitions& operator=(MobDropCatalogDefinitions&&)=default;
    static MobDropCatalogDefinitions load_sql(const storage::ResultSet&);
};
}
