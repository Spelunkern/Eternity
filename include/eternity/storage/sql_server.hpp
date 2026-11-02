#pragma once

#include <sql.h>
#include <sqlext.h>
#include <cstdint>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <variant>

namespace eternity::storage {

struct SqlServerConfig {
    std::string driver;
    std::string server;
    std::uint16_t port = 1433;
    std::string user;
    std::string password;
    std::string database = "PS_GameDefs";
    static SqlServerConfig from_environment();
};

using Cell = std::optional<std::string>;
using Row = std::vector<Cell>;
// Text parameters are UTF-8 and bound as Unicode; procedure varchar conversions remain SQL Server's responsibility.
using Parameter = std::variant<std::monostate, std::string, std::int64_t, double>;
struct ResultSet {
    std::vector<std::string> columns;
    std::vector<Row> rows;
};

// Escape ODBC attribute values, including passwords containing semicolons/braces.
std::string odbc_attribute(std::string_view value);

class SqlServer {
public:
    explicit SqlServer(const SqlServerConfig& config);
    ~SqlServer();
    SqlServer(const SqlServer&) = delete;
    SqlServer& operator=(const SqlServer&) = delete;
    std::vector<ResultSet> query(std::string_view sql, std::span<const Parameter> parameters = {});
    void begin();
    void commit();
    void rollback();
private:
    SQLHENV environment_ = SQL_NULL_HENV;
    SQLHDBC connection_ = SQL_NULL_HDBC;
    bool transaction_active_ = false;
};

} // namespace eternity::storage
