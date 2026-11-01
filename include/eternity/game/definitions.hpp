#pragma once
#include "eternity/storage/sql_server.hpp"
#include <map>
#include <string>

namespace eternity::game {
// Original result columns are retained until each native definition is reconstructed.
// No gameplay semantics or EP5 defaults are inferred here.
struct Definitions {
    std::map<std::string, storage::ResultSet> catalogs;
    static Definitions load(storage::SqlServer& database);
};
}
