#pragma once
#include "eternity/auth/login_protocol.hpp"

namespace eternity::auth {
// Existing GetUser plaintext procedure path. The separate Korea MD5 procedure
// remains unavailable in the backups and is not silently synthesized here.
class SqlAuthenticator {
public:
    explicit SqlAuthenticator(storage::SqlServerConfig config) : config_(std::move(config)) {}
    LoginResult operator()(const Credentials& input, std::uint64_t session_id, const std::string& ip) const;
private:
    storage::SqlServerConfig config_;
};
}
