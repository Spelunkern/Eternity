#include "eternity/storage/sql_server.hpp"
#include "eternity/game/definitions.hpp"
#include <iostream>
#include <string_view>

int main(int argc, char** argv) {
    if (argc == 2 && std::string_view(argv[1]) == "--version") {
        std::cout << "Eternity 0.1.0 (native reconstruction in progress)\n";
        return 0;
    }
    if (argc != 2 || (std::string_view(argv[1]) != "--check-database" &&
                      std::string_view(argv[1]) != "--check-original-loaders")) {
        std::cerr << "Usage: eternity-server --version | --check-database | --check-original-loaders\n"
                     "Gameplay and client endpoints are not implemented yet.\n";
        return 2;
    }
    try {
        eternity::storage::SqlServer database(eternity::storage::SqlServerConfig::from_environment());
        if (std::string_view(argv[1]) == "--check-original-loaders") {
            const auto definitions = eternity::game::Definitions::load(database);
            for (const auto& [name, catalog] : definitions.catalogs)
                std::cout << name << ": " << catalog.rows.size() << " rows, " << catalog.columns.size() << " columns\n";
            std::cout << "Original definition procedures loaded successfully.\n";
            return 0;
        }
        // Read-only inspection of the recovered definition database.
        const auto results = database.query(
            "SELECT 'Mobs' AS Catalog, COUNT_BIG(*) AS Rows FROM dbo.Mobs "
            "UNION ALL SELECT 'Items', COUNT_BIG(*) FROM dbo.Items "
            "UNION ALL SELECT 'Skills', COUNT_BIG(*) FROM dbo.Skills;");
        if (results.size() != 1 || results.front().rows.size() != 3)
            throw std::runtime_error("Unexpected definition catalog result");
        for (const auto& row : results.front().rows) {
            if (row.size() != 2 || !row[0] || !row[1]) throw std::runtime_error("Incomplete catalog row");
            std::cout << *row[0] << ": " << *row[1] << '\n';
        }
        std::cout << "SQL Server definition catalog check passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Eternity: " << error.what() << '\n';
        return 1;
    }
}
