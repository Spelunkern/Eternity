#pragma once
#include "eternity/storage/sql_server.hpp"
#include <cstdint>
#include <string>

namespace eternity::auth {
struct LoginResult {
    // Preserve the original stored procedure's status; packet mapping is separate.
    std::int32_t status;
    std::int32_t user_id;
};

class Accounts {
public:
    explicit Accounts(storage::SqlServer& database) : database_(database) {}
    LoginResult login(const std::string& username, const std::string& password,
                      std::int64_t session_id, const std::string& ipv4);
private:
    storage::SqlServer& database_;
};
}
