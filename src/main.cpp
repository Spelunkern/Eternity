#include "eternity/storage/sql_server.hpp"
#include "eternity/game/definitions.hpp"
#include "eternity/auth/sql_authenticator.hpp"
#include "eternity/net/login_server.hpp"
#include <asio.hpp>
#include <charconv>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <string_view>

int main(int argc, char** argv) {
    if (argc == 2 && std::string_view(argv[1]) == "--version") {
        std::cout << "Eternity 0.1.0 (native reconstruction in progress)\n";
        return 0;
    }
    if (argc != 2 || (std::string_view(argv[1]) != "--check-database" &&
                      std::string_view(argv[1]) != "--check-original-loaders" &&
                      std::string_view(argv[1]) != "--serve-login")) {
        std::cerr << "Usage: eternity-server --version | --check-database | --check-original-loaders | --serve-login\n"
                     "World endpoints and gameplay are not implemented yet.\n";
        return 2;
    }
    try {
        const auto database_config = eternity::storage::SqlServerConfig::from_environment();
        eternity::storage::SqlServer database(database_config);
        if (std::string_view(argv[1]) == "--serve-login") {
            (void)database.query("SELECT 1;");
            eternity::net::LoginServerOptions options;
            if (const char* address=std::getenv("ETERNITY_LOGIN_BIND")) options.address=address;
            if (const char* raw=std::getenv("ETERNITY_LOGIN_PORT")) {
                const std::string_view value(raw);
                unsigned int port=0;
                const auto parsed=std::from_chars(value.data(),value.data()+value.size(),port);
                if (parsed.ec!=std::errc{} || parsed.ptr!=value.data()+value.size() || port>65535)
                    throw std::invalid_argument("ETERNITY_LOGIN_PORT must be in 0..65535");
                options.port=static_cast<std::uint16_t>(port);
            }
            asio::io_context io;
            eternity::net::LoginServer server(io,options,eternity::auth::SqlAuthenticator(database_config));
            asio::signal_set signals(io,SIGINT,SIGTERM);
            signals.async_wait([&server](const asio::error_code& error,int) { if(!error) server.stop(); });
            server.start();
            std::cout<<"Login listening on "<<options.address<<":"<<server.port()
                     <<" (legacy SQL path; worlds offline)."<<std::endl;
            io.run();
            return 0;
        }
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
