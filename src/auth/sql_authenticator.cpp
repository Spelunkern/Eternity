#include "eternity/auth/sql_authenticator.hpp"
#include <limits>

namespace eternity::auth {
LoginResult SqlAuthenticator::operator()(const Credentials& input, std::uint64_t session_id, const std::string& ip) const {
    if (session_id == 0 || session_id > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
        throw std::invalid_argument("Session identifier exceeds the original bigint contract");
    storage::SqlServer database(config_);
    Accounts accounts(database);
    return accounts.login(input.username,input.password,static_cast<std::int64_t>(session_id),ip);
}
}
