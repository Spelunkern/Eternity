#include "eternity/game/definitions.hpp"
#include <array>

namespace eternity::game {
Definitions Definitions::load(storage::SqlServer& database) {
    struct Loader { const char* name; const char* query; const char* key; };
    // Original PS_GameDefs procedures, retained rather than rewritten for another engine.
    constexpr std::array loaders = {
        Loader{"Mobs", "EXEC dbo.usp_Read_Mobs_R;", "MobID"},
        Loader{"Items", "EXEC dbo.usp_Read_Items_R;", "ItemID"},
        Loader{"Skills", "EXEC dbo.usp_Read_Skills_R;", "SkillID"},
        Loader{"MobItems", "EXEC dbo.usp_Read_MobItems_R;", nullptr},
        Loader{"MobSkills", "EXEC dbo.usp_Read_MobSkills_R;", nullptr},
        Loader{"BaseItemDefs", "EXEC dbo.usp_Read_BaseItemDefs_R;", nullptr},
        Loader{"ExpDefs", "EXEC dbo.usp_Read_ExpDefs_R;", nullptr},
        Loader{"StatusDefs", "EXEC dbo.usp_Read_StatusDefs_R;", nullptr},
    };
    Definitions definitions;
    for (const auto& loader : loaders) {
        auto results = database.query(loader.query);
        if (results.size() != 1 || results.front().columns.empty())
            throw std::runtime_error(std::string("Unexpected result shape from ") + loader.name);
        // Skill IDs repeat across levels; preserve ordered rows and original identifiers.
        // Require the identifier column without inventing a new identifier scheme.
        if (loader.key) {
            bool found = false;
            for (const auto& column : results.front().columns) found = found || column == loader.key;
            if (!found) throw std::runtime_error(std::string("Missing original identifier in ") + loader.name);
        }
        definitions.catalogs.emplace(loader.name, std::move(results.front()));
    }
    return definitions;
}
}
