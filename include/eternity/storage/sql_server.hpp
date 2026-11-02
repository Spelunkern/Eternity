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
#include <array>
#include <utility>

namespace eternity::game {class CharacterSaveStore;}
namespace eternity::storage {

// First ODBC diagnostic, corresponding to original SDatabase::GetErrorMsg.
// Preserve all formatted diagnostics in what(); native codes remain structured.
class SqlError:public std::runtime_error {
public:
    SqlError(std::string message,std::string state,std::int32_t native):std::runtime_error(std::move(message)),sqlstate(std::move(state)),native_error(native) {}
    const std::string sqlstate;const std::int32_t native_error;
};

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
    bool transaction_active() const noexcept { return transaction_active_; }
    // Original SaveCharacter ignores BeginTrans failure and still executes its
    // phases. Only its coordinator can open that legacy write context.
    bool character_write_context_active() const noexcept {return transaction_active_ || legacy_character_save_;}
    // SDatabase query diagnostics belong to the connection and survive successful
    // calls. Compatibility adapters can report equivalent pre-query SQL errors.
    std::string_view last_query_sqlstate() const noexcept {return {query_state_.data(),query_state_[0]?5U:0U};}
    std::int32_t last_query_native_error() const noexcept {return query_native_error_;}
    void observe_query_error(std::string_view diagnostic) noexcept;
private:
    friend class eternity::game::CharacterSaveStore;
    SQLHENV environment_ = SQL_NULL_HENV;
    SQLHDBC connection_ = SQL_NULL_HDBC;
    bool transaction_active_ = false;
    bool legacy_character_save_ = false;
    std::array<char,5> query_state_{};
    std::int32_t query_native_error_=0;
};

} // namespace eternity::storage
