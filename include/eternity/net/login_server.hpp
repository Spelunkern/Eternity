#pragma once
#include "eternity/auth/login_protocol.hpp"
#include <asio/io_context.hpp>
#include <functional>

namespace eternity::session { class Registry; }

namespace eternity::net {
struct LoginServerOptions {
    std::string address = "127.0.0.1";
    std::uint16_t port = 30800;
    std::size_t maximum_connections = 128;
    std::size_t database_workers = 2;
    std::vector<auth::Realm> realms = {{0,2,0,1000,"Eternity"}};
};

using Authenticate = std::function<auth::LoginResult(const auth::Credentials&, std::uint64_t, const std::string&)>;

// Socket state and output queues are serialized on one Asio strand; SQL runs off it.
class LoginServer {
public:
    LoginServer(asio::io_context& io, LoginServerOptions options, Authenticate authenticate,
                std::shared_ptr<session::Registry> sessions = {}, std::shared_ptr<auth::RealmDirectory> realms = {});
    ~LoginServer();
    LoginServer(const LoginServer&) = delete;
    LoginServer& operator=(const LoginServer&) = delete;
    void start();
    void stop();
    std::uint16_t port() const;
    std::size_t session_count() const;
private:
    struct Impl;
    std::shared_ptr<Impl> impl_;
};
}
