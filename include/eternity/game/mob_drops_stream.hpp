#pragma once
#include "eternity/game/mob_drops_reload.hpp"
namespace eternity::game {
// Compose the existing completed-row reload engine with the actual retained ODBC
// typed cursor. Original SQL NULL buffer contents are not guessed: NULL fails
// this projection and leaves the current row unpublished.
bool reload_mob_drop_stream(MobDropDefinitions&,std::int32_t& changes,
    storage::TypedSqlCursor&,MobDropReloadDiagnostics&);
}
