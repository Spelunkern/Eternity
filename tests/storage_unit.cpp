#include "eternity/storage/sql_server.hpp"
#include <cstdlib>
#include <iostream>
#include <stdexcept>

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

int main() {
    using namespace eternity::storage;
    try {
        require(odbc_attribute("p;ass}word") == "{p;ass}}word}", "ODBC attribute escaping failed");
        setenv("ETERNITY_DB_USER", "test", 1);
        setenv("ETERNITY_DB_PASSWORD", "not-a-real-secret", 1);
        setenv("ETERNITY_DB_PORT", "14345", 1);
        require(SqlServerConfig::from_environment().port == 14345, "Custom port not read");
        for (const char* value : {"", "-1", "0", "65536", "1433extra"}) {
            setenv("ETERNITY_DB_PORT", value, 1);
            bool rejected = false;
            try { (void)SqlServerConfig::from_environment(); }
            catch (const std::invalid_argument&) { rejected = true; }
            require(rejected, "Invalid database port accepted");
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
