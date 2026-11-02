#pragma once
#include <string>

namespace eternity::game {
// ps_dbAgent's original CRT %f: 17 significant digits, then six decimal places.
// Bind through SQL DECIMAL(38,6) before REAL to preserve literal conversion rules.
std::string legacy_sql_coordinate(float value);
}
