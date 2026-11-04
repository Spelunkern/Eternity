#pragma once
#include "eternity/game/mob_drops.hpp"

namespace eternity::game {
enum class MobDropReloadRead {row,end,failed};
enum class MobDropReloadFailure {query,row};
class MobDropReloadCursor {
public:
    virtual ~MobDropReloadCursor()=default;
    // Publish a row only after all four original columns were read successfully.
    virtual MobDropReloadRead next(MobDropRow&)=0;
};
class MobDropReloadDiagnostics {
public:
    virtual ~MobDropReloadDiagnostics()=default;
    virtual void rejected(const MobDropRow&)=0;
    virtual void failed(MobDropReloadFailure)=0;
};
// ps_game.exe CGameData::ReLoadConstMobItem 004c2e70. Does not clear missing
// rows, resize storage or reset the caller's cumulative counter. Grade and rate
// each increment that signed counter with DWORD wrap when actually changed.
// Earlier rows remain published on read failure. Run under the definition owner;
// diagnostics/cursor must retain storage and all existing definition bindings.
bool reload_mob_drop_rows(MobDropDefinitions&,std::int32_t& changes,MobDropReloadCursor&,MobDropReloadDiagnostics&);
// Lazy conversion over a buffered procedure result preserves prefixes on row
// conversion failure. NULL has no deterministic original initial-buffer value
// and is rejected explicitly. Original query contract has four positional columns.
bool reload_mob_drop_result(MobDropDefinitions&,const storage::ResultSet&,std::int32_t& changes,MobDropReloadDiagnostics&);
// Read-only original usp_Read_MobItems_R2 request on the supplied native SQL
// owner. No database/pool lifecycle, resets or implicit reconnects. Uses the
// retained typed SQL cursor: completed rows survive later fetch/GetData failure.
// The caller serializes the connection and retained definition owner.
bool reload_mob_drop_sql(MobDropDefinitions&,std::uint16_t request,std::int32_t& changes,storage::SqlServer&,MobDropReloadDiagnostics&);
}
