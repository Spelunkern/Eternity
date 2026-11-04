#include "eternity/storage/sql_server.hpp"
#include <codecvt>
#include <locale>
#include <algorithm>
#include <array>
#include <charconv>
#include <cstdlib>
#include <limits>
#include <memory>

namespace eternity::storage {
namespace {
std::string variable(const char* name, std::string fallback = {}) {
    const char* value = std::getenv(name);
    return value ? value : fallback;
}

void check(SQLRETURN result, SQLSMALLINT type, SQLHANDLE handle, std::string_view operation) {
    if (SQL_SUCCEEDED(result)) return;
    std::string message(operation);
    std::array<SQLCHAR, 6> state{};
    std::array<SQLCHAR, 2048> detail{};
    SQLINTEGER native = 0;
    SQLSMALLINT size = 0;std::string first_state;SQLINTEGER first_native=0;
    for (SQLSMALLINT index = 1; SQL_SUCCEEDED(SQLGetDiagRec(type, handle, index,
         state.data(), &native, detail.data(), static_cast<SQLSMALLINT>(detail.size()), &size)); ++index) {
        if(index==1) {first_state=reinterpret_cast<char*>(state.data());first_native=native;}
        message += " [" + std::string(reinterpret_cast<char*>(state.data())) + "] ";
        message += reinterpret_cast<char*>(detail.data());
    }
    throw SqlError(std::move(message),std::move(first_state),first_native);
}

struct Statement {
    SQLHSTMT handle = SQL_NULL_HSTMT;
    explicit Statement(SQLHDBC connection) {
        check(SQLAllocHandle(SQL_HANDLE_STMT, connection, &handle), SQL_HANDLE_DBC, connection, "allocate statement");
        try {
            check(SQLSetStmtAttr(handle, SQL_ATTR_QUERY_TIMEOUT, reinterpret_cast<SQLPOINTER>(30), 0),
                  SQL_HANDLE_STMT, handle, "set query timeout");
        } catch (...) {
            SQLFreeHandle(SQL_HANDLE_STMT, handle);
            throw;
        }
    }
    ~Statement() { if (handle != SQL_NULL_HSTMT) SQLFreeHandle(SQL_HANDLE_STMT, handle); }
};
SQLRETURN execute_statement(Statement& statement,std::string_view sql,std::span<const Parameter> parameters,
    std::vector<SQLLEN>& indicators,std::vector<std::vector<SQLWCHAR>>& wide_parameters) {
        if (sql.size() > static_cast<std::size_t>(std::numeric_limits<SQLINTEGER>::max()))
            throw std::invalid_argument("SQL query is too long");
        if (parameters.size() > std::numeric_limits<SQLUSMALLINT>::max())
            throw std::invalid_argument("Too many SQL parameters");
        SQLRETURN result = SQL_SUCCESS;
        if (parameters.empty()) {
            result = SQLExecDirect(statement.handle,
                reinterpret_cast<SQLCHAR*>(const_cast<char*>(sql.data())), static_cast<SQLINTEGER>(sql.size()));
        } else {
            result = SQLPrepare(statement.handle,
                reinterpret_cast<SQLCHAR*>(const_cast<char*>(sql.data())), static_cast<SQLINTEGER>(sql.size()));
            check(result, SQL_HANDLE_STMT, statement.handle, "prepare query");
            for (std::size_t index = 0; index < parameters.size(); ++index) {
                SQLSMALLINT c_type = SQL_C_CHAR, sql_type = SQL_VARCHAR;
                SQLULEN column_size = 1;
                SQLLEN buffer_size = 0;
                SQLPOINTER value = nullptr;
                const Parameter& parameter = parameters[index];
                if (const auto* text = std::get_if<std::string>(&parameter)) {
                    // Bind Unicode explicitly. SQL_VARCHAR can interpret UTF-8 bytes using the server's ANSI collation.
                    std::wstring_convert<std::codecvt_utf8_utf16<char16_t>,char16_t> converter;
                    const auto utf16=converter.from_bytes(*text);
                    if(converter.converted()!=text->size()) throw std::invalid_argument("SQL text parameter is not complete UTF-8");
                    auto& wide=wide_parameters[index];wide.assign(utf16.begin(),utf16.end());
                    if (wide.size() > static_cast<std::size_t>(std::numeric_limits<SQLLEN>::max())/sizeof(SQLWCHAR))
                        throw std::invalid_argument("SQL parameter is too long");
                    c_type=SQL_C_WCHAR;sql_type=SQL_WVARCHAR;
                    column_size = std::max<SQLULEN>(1, wide.size());
                    buffer_size = static_cast<SQLLEN>(wide.size()*sizeof(SQLWCHAR));
                    indicators[index] = buffer_size;
                    wide.push_back(0);
                    value = wide.data();
                } else if (const auto* integer = std::get_if<std::int64_t>(&parameter)) {
                    c_type = SQL_C_SBIGINT;
                    sql_type = SQL_BIGINT;
                    column_size = 19;
                    buffer_size = sizeof(*integer);
                    value = const_cast<std::int64_t*>(integer);
                } else if (const auto* real = std::get_if<double>(&parameter)) {
                    c_type = SQL_C_DOUBLE;
                    sql_type = SQL_DOUBLE;
                    column_size = 15;
                    buffer_size = sizeof(*real);
                    value = const_cast<double*>(real);
                } else {
                    indicators[index] = SQL_NULL_DATA;
                }
                check(SQLBindParameter(statement.handle, static_cast<SQLUSMALLINT>(index + 1), SQL_PARAM_INPUT,
                      c_type, sql_type, column_size, 0, value, buffer_size, &indicators[index]),
                      SQL_HANDLE_STMT, statement.handle, "bind query parameter");
            }
            result = SQLExecute(statement.handle);
        }
        if (result != SQL_NO_DATA) check(result, SQL_HANDLE_STMT, statement.handle, "execute query");
        return result;

}

}

struct SqlConnectionLifetime {
    SQLHENV environment=SQL_NULL_HENV;SQLHDBC connection=SQL_NULL_HDBC;
    SqlServer* facade=nullptr;bool rollback=false,cursor_active=false;
    ~SqlConnectionLifetime() {
        if(connection!=SQL_NULL_HDBC) {
            if(rollback) SQLEndTran(SQL_HANDLE_DBC,connection,SQL_ROLLBACK);
            SQLDisconnect(connection);SQLFreeHandle(SQL_HANDLE_DBC,connection);
        }
        if(environment!=SQL_NULL_HENV) SQLFreeHandle(SQL_HANDLE_ENV,environment);
    }
};

std::string odbc_attribute(std::string_view value) {
    std::string escaped = "{";
    for (char character : value) {
        escaped += character;
        if (character == '}') escaped += '}';
    }
    return escaped + "}";
}

SqlServerConfig SqlServerConfig::from_environment() {
    SqlServerConfig config;
    config.driver = variable("ETERNITY_DB_DRIVER", "/usr/lib/x86_64-linux-gnu/odbc/libtdsodbc.so");
    config.server = variable("ETERNITY_DB_SERVER", "127.0.0.1");
    config.user = variable("ETERNITY_DB_USER");
    config.password = variable("ETERNITY_DB_PASSWORD");
    config.database = variable("ETERNITY_DB_DATABASE", "PS_GameDefs");
    const auto text = variable("ETERNITY_DB_PORT", "1433");
    unsigned int port = 0;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), port);
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() || port == 0 || port > 65535)
        throw std::invalid_argument("ETERNITY_DB_PORT must be an integer in 1..65535");
    config.port = static_cast<std::uint16_t>(port);
    if (config.user.empty() || config.password.empty())
        throw std::invalid_argument("ETERNITY_DB_USER and ETERNITY_DB_PASSWORD are required");
    return config;
}

SqlServer::SqlServer(const SqlServerConfig& config) {
    try {
        check(SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &environment_), SQL_HANDLE_ENV,
              environment_, "allocate ODBC environment");
        check(SQLSetEnvAttr(environment_, SQL_ATTR_ODBC_VERSION, reinterpret_cast<SQLPOINTER>(SQL_OV_ODBC3), 0),
              SQL_HANDLE_ENV, environment_, "set ODBC version");
        check(SQLAllocHandle(SQL_HANDLE_DBC, environment_, &connection_), SQL_HANDLE_ENV,
              environment_, "allocate connection");
        check(SQLSetConnectAttr(connection_, SQL_LOGIN_TIMEOUT, reinterpret_cast<SQLPOINTER>(10), 0),
              SQL_HANDLE_DBC, connection_, "set login timeout");
        std::string attributes = "DRIVER=" + odbc_attribute(config.driver) + ";SERVER=" +
            odbc_attribute(config.server) + ";PORT=" + std::to_string(config.port) +
            ";UID=" + odbc_attribute(config.user) + ";PWD=" + odbc_attribute(config.password) +
            ";DATABASE=" + odbc_attribute(config.database) + ";TDS_Version=7.4;ClientCharset=UTF-8;";
        if (attributes.size() > static_cast<std::size_t>(std::numeric_limits<SQLSMALLINT>::max()))
            throw std::invalid_argument("ODBC connection attributes are too long");
        check(SQLDriverConnect(connection_, nullptr, reinterpret_cast<SQLCHAR*>(attributes.data()),
              static_cast<SQLSMALLINT>(attributes.size()), nullptr, 0, nullptr, SQL_DRIVER_NOPROMPT),
              SQL_HANDLE_DBC, connection_, "connect to SQL Server");
        lifetime_=std::make_shared<SqlConnectionLifetime>();
        lifetime_->environment=environment_;lifetime_->connection=connection_;lifetime_->facade=this;
    } catch (...) {
        if (connection_ != SQL_NULL_HDBC) SQLFreeHandle(SQL_HANDLE_DBC, connection_);
        if (environment_ != SQL_NULL_HENV) SQLFreeHandle(SQL_HANDLE_ENV, environment_);
        throw;
    }
}

SqlServer::~SqlServer() {
    // The final cursor frees its statement before the final connection lease.
    if(lifetime_) {lifetime_->facade=nullptr;lifetime_->rollback=transaction_active_;}
}

void SqlServer::begin() {
    if(lifetime_ && lifetime_->cursor_active) throw std::logic_error("SQL connection has an active cursor");
    if (transaction_active_) throw std::logic_error("A transaction is already active");
    check(SQLSetConnectAttr(connection_, SQL_ATTR_AUTOCOMMIT,
          reinterpret_cast<SQLPOINTER>(SQL_AUTOCOMMIT_OFF), 0), SQL_HANDLE_DBC, connection_, "begin transaction");
    transaction_active_ = true;
}

void SqlServer::commit() {
    if(lifetime_ && lifetime_->cursor_active) throw std::logic_error("SQL connection has an active cursor");
    if (!transaction_active_) throw std::logic_error("No transaction is active");
    check(SQLEndTran(SQL_HANDLE_DBC, connection_, SQL_COMMIT), SQL_HANDLE_DBC, connection_, "commit transaction");
    check(SQLSetConnectAttr(connection_, SQL_ATTR_AUTOCOMMIT,
          reinterpret_cast<SQLPOINTER>(SQL_AUTOCOMMIT_ON), 0), SQL_HANDLE_DBC, connection_, "restore autocommit");
    transaction_active_ = false;
}

void SqlServer::rollback() {
    if(lifetime_ && lifetime_->cursor_active) throw std::logic_error("SQL connection has an active cursor");
    if (!transaction_active_) throw std::logic_error("No transaction is active");
    // FreeTDS needs a successful server rollback before restoring autocommit.
    // An early conversion failure may precede its implicit BEGIN, and server
    // errors may already have ended the transaction. An empty BEGIN makes
    // those cases rollback normally without risking a commit of pending work.
    query("IF @@TRANCOUNT=0 BEGIN TRANSACTION;");
    check(SQLEndTran(SQL_HANDLE_DBC, connection_, SQL_ROLLBACK), SQL_HANDLE_DBC, connection_, "rollback transaction");
    check(SQLSetConnectAttr(connection_, SQL_ATTR_AUTOCOMMIT,
          reinterpret_cast<SQLPOINTER>(SQL_AUTOCOMMIT_ON), 0), SQL_HANDLE_DBC, connection_, "restore autocommit");
    transaction_active_ = false;
}

std::vector<ResultSet> SqlServer::query(std::string_view sql, std::span<const Parameter> parameters) {
    if(lifetime_ && lifetime_->cursor_active) throw std::logic_error("SQL connection has an active cursor");
    try {
        Statement statement(connection_);
        std::vector<SQLLEN> indicators(parameters.size());
        std::vector<std::vector<SQLWCHAR>> wide_parameters(parameters.size());
        auto result=execute_statement(statement,sql,parameters,indicators,wide_parameters);
        std::vector<ResultSet> sets;
        while (result != SQL_NO_DATA) {
            SQLSMALLINT count = 0;
            check(SQLNumResultCols(statement.handle, &count), SQL_HANDLE_STMT, statement.handle, "read columns");
            if (count > 0) {
                ResultSet set;
                for (SQLUSMALLINT column = 1; column <= static_cast<SQLUSMALLINT>(count); ++column) {
                    std::array<SQLCHAR, 256> name{};
                    SQLSMALLINT length = 0, type = 0, decimals = 0, nullable = 0;
                    SQLULEN precision = 0;
                    check(SQLDescribeCol(statement.handle, column, name.data(), static_cast<SQLSMALLINT>(name.size()),
                        &length, &type, &precision, &decimals, &nullable), SQL_HANDLE_STMT, statement.handle, "describe column");
                    set.columns.emplace_back(reinterpret_cast<char*>(name.data()));
                }
                while ((result = SQLFetch(statement.handle)) != SQL_NO_DATA) {
                    check(result, SQL_HANDLE_STMT, statement.handle, "fetch row");
                    Row row;
                    for (SQLUSMALLINT column = 1; column <= static_cast<SQLUSMALLINT>(count); ++column) {
                        Cell cell = std::string{};
                        for (;;) {
                            std::array<char, 4096> buffer{};
                            SQLLEN indicator = 0;
                            const auto fetched = SQLGetData(statement.handle, column, SQL_C_CHAR,
                                buffer.data(), static_cast<SQLLEN>(buffer.size()), &indicator);
                            if (fetched == SQL_NO_DATA) break;
                            check(fetched, SQL_HANDLE_STMT, statement.handle, "read cell");
                            if (indicator == SQL_NULL_DATA) { cell.reset(); break; }
                            *cell += buffer.data();
                            if (fetched == SQL_SUCCESS) break;
                        }
                        row.push_back(std::move(cell));
                    }
                    set.rows.push_back(std::move(row));
                }
                sets.push_back(std::move(set));
            }
            result = SQLMoreResults(statement.handle);
            if (result != SQL_NO_DATA) check(result, SQL_HANDLE_STMT, statement.handle, "advance result set");
        }
        return sets;
    }catch(const SqlError& error) {
        record_error(error);throw;
    }catch(const std::runtime_error& error) {observe_query_error(error.what());throw;}
}
void SqlServer::observe_query_error(std::string_view diagnostic) noexcept {
    const auto at=diagnostic.find('[');if(at==std::string_view::npos || at+6>=diagnostic.size() || diagnostic[at+6]!=']') return;
    const auto state=diagnostic.substr(at+1,5);if(std::any_of(state.begin(),state.end(),[](unsigned char c){return !((c>='0'&&c<='9') || (c>='A'&&c<='Z'));})) return;
    std::copy(state.begin(),state.end(),query_state_.begin());
}
} // namespace eternity::storage

namespace eternity::storage {
void SqlServer::record_error(const SqlError& error) {
    query_state_.fill(0);
    if(error.sqlstate.size()==5) std::copy(error.sqlstate.begin(),error.sqlstate.end(),query_state_.begin());
    query_native_error_=error.native_error;
}
struct TypedSqlCursor::Impl {
    std::shared_ptr<SqlConnectionLifetime> owner;
    std::vector<Parameter> parameters;
    std::vector<SQLLEN> indicators;
    std::vector<std::vector<SQLWCHAR>> wide_parameters;
    std::vector<SqlColumnType> columns;
    std::unique_ptr<Statement> statement; // Freed before bound parameter buffers.
    std::optional<SqlError> failure;
    bool ended=false;
    ~Impl() {statement.reset();owner->cursor_active=false;}
    Impl(std::shared_ptr<SqlConnectionLifetime> retained,std::string_view sql,std::span<const Parameter> values,std::span<const SqlColumnType> types)
        :owner(std::move(retained)),parameters(values.begin(),values.end()),indicators(values.size()),
         wide_parameters(values.size()),columns(types.begin(),types.end()),statement(std::make_unique<Statement>(owner->connection)) {
        if(columns.empty() || columns.size()>static_cast<std::size_t>(std::numeric_limits<SQLSMALLINT>::max())) throw std::invalid_argument("Invalid typed SQL column projection");
        auto status=execute_statement(*statement,sql,parameters,indicators,wide_parameters);
        while(status!=SQL_NO_DATA) {
            SQLSMALLINT count=0;check(SQLNumResultCols(statement->handle,&count),SQL_HANDLE_STMT,statement->handle,"read typed columns");
            if(count>0) {
                if(static_cast<std::size_t>(count)!=columns.size()) throw SqlError("Typed SQL result column count differs","07005",0);
                return;
            }
            status=SQLMoreResults(statement->handle);
            if(status!=SQL_NO_DATA) check(status,SQL_HANDLE_STMT,statement->handle,"advance typed result");
        }
        ended=true;
    }
};
TypedSqlCursor::TypedSqlCursor(std::unique_ptr<Impl> impl):impl_(std::move(impl)) {}
TypedSqlCursor::~TypedSqlCursor()=default;
TypedSqlCursor::TypedSqlCursor(TypedSqlCursor&&) noexcept=default;
TypedSqlCursor& TypedSqlCursor::operator=(TypedSqlCursor&&) noexcept=default;
const SqlError* TypedSqlCursor::error() const noexcept {return impl_ && impl_->failure?&*impl_->failure:nullptr;}
SqlCursorRead TypedSqlCursor::next(TypedSqlRow& output) {
    if(!impl_) throw std::logic_error("Moved SQL cursor");
    auto& cursor=*impl_;if(cursor.failure) return SqlCursorRead::error;if(cursor.ended) return SqlCursorRead::end;
    try {
        const auto fetched=SQLFetch(cursor.statement->handle);
        if(fetched==SQL_NO_DATA) {
            for(;;) {
                const auto status=SQLMoreResults(cursor.statement->handle);
                if(status==SQL_NO_DATA) break;
                check(status,SQL_HANDLE_STMT,cursor.statement->handle,"finish typed result");
                SQLSMALLINT count=0;check(SQLNumResultCols(cursor.statement->handle,&count),SQL_HANDLE_STMT,cursor.statement->handle,"read trailing columns");
                if(count>0) throw SqlError("Unexpected additional typed SQL result","07005",0);
            }
            cursor.ended=true;return SqlCursorRead::end;
        }
        check(fetched,SQL_HANDLE_STMT,cursor.statement->handle,"fetch typed row");
        TypedSqlRow row;row.reserve(cursor.columns.size());
        for(std::size_t index=0;index<cursor.columns.size();++index) {
            SQLSMALLINT short_value=0;SQLCHAR tiny_value=0;SQLINTEGER long_value=0;
            SQLSMALLINT type=SQL_C_SHORT;SQLPOINTER value=&short_value;SQLLEN width=sizeof(short_value),indicator=0;
            if(cursor.columns[index]==SqlColumnType::tiny_integer) {type=SQL_C_TINYINT;value=&tiny_value;width=sizeof(tiny_value);}
            else if(cursor.columns[index]==SqlColumnType::long_integer) {type=SQL_C_LONG;value=&long_value;width=sizeof(long_value);}
            check(SQLGetData(cursor.statement->handle,static_cast<SQLUSMALLINT>(index+1),type,value,width,&indicator),SQL_HANDLE_STMT,cursor.statement->handle,"read typed cell");
            if(indicator==SQL_NULL_DATA) row.emplace_back(std::nullopt);
            else row.emplace_back(type==SQL_C_SHORT?static_cast<std::int64_t>(short_value):type==SQL_C_TINYINT?static_cast<std::int64_t>(tiny_value):static_cast<std::int64_t>(long_value));
        }
        output=std::move(row);return SqlCursorRead::row;
    }catch(const SqlError& error) {
        cursor.failure.emplace(error);if(cursor.owner->facade) cursor.owner->facade->record_error(error);
        return SqlCursorRead::error;
    }
}
TypedSqlCursor SqlServer::cursor(std::string_view sql,std::span<const Parameter> parameters,std::span<const SqlColumnType> columns) {
    if(!lifetime_) throw std::logic_error("SQL connection is not available");
    if(lifetime_->cursor_active) throw std::logic_error("SQL connection has an active cursor");
    try {
        auto impl=std::make_unique<TypedSqlCursor::Impl>(lifetime_,sql,parameters,columns);
        lifetime_->cursor_active=true;return TypedSqlCursor(std::move(impl));
    }
    catch(const SqlError& error) {record_error(error);throw;}
    catch(const std::runtime_error& error) {observe_query_error(error.what());throw;}
}
}
