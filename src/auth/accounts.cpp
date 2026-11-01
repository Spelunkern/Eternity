#include "eternity/auth/accounts.hpp"
#include <array>
#include <charconv>
#include <limits>

namespace eternity::auth {
namespace {
std::int32_t integer(const storage::Cell& cell) {
    if (!cell) throw std::runtime_error("Login returned NULL integer");
    std::int32_t number = 0;
    const auto result = std::from_chars(cell->data(), cell->data() + cell->size(), number);
    if (result.ec != std::errc{} || result.ptr != cell->data() + cell->size())
        throw std::runtime_error("Login returned invalid integer");
    return number;
}

void validate(const std::string& value, std::size_t maximum, const char* name) {
    if (value.size() > maximum || value.find('\0') != std::string::npos)
        throw std::invalid_argument(std::string(name) + " exceeds the original SQL parameter contract");
}
}

LoginResult Accounts::login(const std::string& username, const std::string& password,
                           std::int64_t session_id, const std::string& ipv4) {
    validate(username, 18, "Username");
    validate(password, 18, "Password");
    validate(ipv4, 15, "IPv4 address");
    const std::array<storage::Parameter, 4> parameters = {username, password, session_id, ipv4};
    const auto result = database_.query(
        "EXEC PS_UserData.dbo.usp_Try_GameLogin3_R @UserID=?, @InPassword=?, @SessionID=?, @UserIP=?;", parameters);
    if (result.size() != 1 || result[0].rows.size() != 1 || result[0].columns.size() != 2)
        throw std::runtime_error("Original login procedure returned an unexpected result shape");
    if (result[0].columns[0] != "Status" || result[0].columns[1] != "UserUID")
        throw std::runtime_error("Original login procedure returned unexpected columns");
    return {integer(result[0].rows[0][0]), integer(result[0].rows[0][1])};
}
}
